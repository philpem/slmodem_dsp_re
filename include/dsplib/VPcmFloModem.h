/*
 * VPcmFloModem.h -- the V.90/V.92 modem's face to the V.34 handshake.
 *
 * Reconstructed from dsplibs.o.  The class has twenty-six members in the blob
 * and this tree writes SIX of them out of line -- `getUinfoValue`,
 * `setPhaseIIinfo`, `getV90CpBits`, `getV90JaBits`, `setPcmSessionType` and
 * `enterPhase3` -- plus `externalReset`, the two `setV34BaudFor*`, the three
 * visual diagnostics, and both entry points, `runPcmModem` and
 * `v90RunDemodulator`.  Seven more are reconstructed as `inline` bodies that
 * the two entry points inline exactly as the object does; see the block that
 * lists them below.
 * Everything else is left undeclared rather than declared-and-undefined,
 * because nothing here calls it and a declaration nobody needs is a claim
 * nobody checked.
 *
 * NOT POLYMORPHIC.  tools/cppstruct.py lists the destructor with the `D1` and
 * `D2` variants and no `D0`, and GCC emits a deleting destructor only for a
 * virtual one, so +0x00 is a real member and there is no vptr (finding F228 is
 * the four classes where that is not true).
 *
 * ===========================================================================
 * THE OBJECT REALLY IS THIRTY-TWO KILOBYTES
 * ===========================================================================
 *
 * docs/v90cpp.md used to say that VPcmFloModem "reaches +32,612, which almost
 * certainly means it indexes *through* `this` into an enclosing session object
 * rather than being that large".  That is finding F268's warning applied in
 * good faith, and for this class it is WRONG.  Run finding F268's own check --
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
 *      is the same signature finding F268 used for the two demodulators.
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
 * The mangling never carries a data member's name (finding F226).  Where the
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

/* The three getters below take one; include/dsplib/int_complex.h defines it. */
struct int_complex;

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
	 * The destructor -- D2 at 0xd030 and D1 at 0xd0a0, 97 bytes each.
	 * The body is empty: the 97 bytes are the six member destructions
	 * the compiler generates for the six typed embedded members below,
	 * in reverse declaration order, which is exactly the blob's call
	 * sequence.  VPcmFloModem.cpp defines it.
	 */
	~VPcmFloModem();

	/*
	 * internalReset -- 0xd4f0, 165 bytes of constant stores: the
	 * transmit-side bookkeeping back to its phase 3 entry values, with
	 * the V.90 baud allow list (index 5 barred).  VPcmFloModem.cpp.
	 */
	void internalReset();

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
	 * --- SEVEN MEMBERS `v90RunDemodulator` INLINES ----------------------
	 *
	 * Each is a `T` symbol of its own in the blob with NO incoming
	 * relocation anywhere in the object, and each one's body appears
	 * open-coded inside `v90RunDemodulator` -- which is what a call the
	 * compiler inlined looks like, since the out-of-line copy has to be
	 * emitted for a non-inline member whether anything reaches it or not.
	 *
	 *   d110  setTerminateJaFlag(unsigned char)          45 B
	 *   d140  setTerminateCpFlag(unsigned char)          45 B
	 *   d170  setTerminateCpNotFlag(unsigned char)       45 B
	 *   d1a0  setMinNofTransmitSequences(unsigned short) 26 B
	 *   d1c0  setNofBitsPhase4(unsigned int)             56 B
	 *   d200  resetBitPointer()                          51 B
	 *   d5a0  copyMpInfoForInterface()                  183 B
	 *
	 * THE `inline` HAS BEEN DROPPED AND ALL SEVEN SYMBOLS ARE CLAIMED
	 * (this paragraph used to price that move at "451 bytes and seven
	 * differential tests"; t_vpcmleaves.cpp is the tests).  The fourteen
	 * inlined sites in `v90RunDemodulator` are unchanged -- GCC still
	 * inlines a same-TU callee at -O3 -- and the out-of-line copies now
	 * exist as the blob has them.
	 */
	void setTerminateJaFlag(unsigned char v);
	void setTerminateCpFlag(unsigned char v);
	void setTerminateCpNotFlag(unsigned char v);
	void setMinNofTransmitSequences(unsigned short n);
	void setNofBitsPhase4(unsigned int constel);
	void resetBitPointer();
	void copyMpInfoForInterface();

	/*
	 * --- THE THREE VISUAL DIAGNOSTICS -----------------------------------
	 *
	 * `VPcmV34GetVisualDiagnostics` dispatches to these three for a PCM
	 * session; see src/pump/v34/v34diag.cpp for the dispatch and
	 * include/dsplib/int_complex.h for what a point is.
	 *
	 * ARGUMENT TYPES ARE THE MANGLING'S and exact --
	 * `P11int_complexm` is `(int_complex *, unsigned long)`.  RETURN
	 * TYPES ARE NOT MANGLED (docs/v90cpp.md); all three leave the number
	 * of points written in %eax on every path, including zero on the
	 * paths that write none, and `unsigned long` is that count with the
	 * same width and signedness as the bound it was clamped against.  A
	 * `size_t` or an `unsigned int` return would compile identically.
	 *
	 * ALL THREE ANSWER NOTHING UNLESS A PCM RECEIVER IS RUNNING: the
	 * first thing each does is `if (pcmSessionType != 0 && info0Layout
	 * == 0) return 0`, which is the object's `test`/`je` pair at 0xf3c1
	 * and 0xf3cd and its twins.
	 */
	unsigned long getConstellation(int_complex *points,
				       unsigned long maxCount);
	unsigned long getLinearEqualizer(int_complex *points,
					 unsigned long maxCount);
	unsigned long getDFE(int_complex *points, unsigned long maxCount);

	/*
	 * --- THE FOUR `VPcmV34Progress` ENTRY POINTS, AND ALL FOUR ARE NOW
	 * --- WRITTEN --------------------------------------------------------
	 *
	 * This block used to be headed "FOUR MEMBERS THIS TREE HAS NOT
	 * WRITTEN".  `runPcmModem` and `v90RunDemodulator` were written first,
	 * and `qcLineVerification` and `vPcmResetPhase3Modem` close the set --
	 * all four are in src/pump/v90/VPcmFloModem.cpp.
	 *
	 * THE WEAK ARRANGEMENT BELOW STAYS AND IS NOW A NO-OP THAT COSTS
	 * NOTHING.  A weak DECLARATION whose symbol is defined at link time
	 * resolves to the definition, so `v34pcmmain.cpp`'s guard now passes
	 * at every one of the four sites and the calls happen -- which is what
	 * the blob does unconditionally.  Removing the macro would be a change
	 * to the one translation unit that has to keep working if a future
	 * split ever takes a member back out, so it is left alone.
	 *
	 * `VPcmV34Progress` calls all four and nothing else does.  They are
	 * marked WEAK in the one
	 * translation unit that calls them -- `src/pump/v34/v34pcmmain.cpp`
	 * defines `DSPLIB_VPCMFLO_UNWRITTEN` before including this file -- so
	 * the reference resolves to zero rather than failing the link of all
	 * 78 test binaries, and the caller tests the pointer-to-member before
	 * it calls through it.  `include/dsplib/vpcm.h` carries the same
	 * arrangement for the five `VPcmV34*` entry points and says why at
	 * length; the rule is the same one, one level further down.
	 *
	 * A TU that DEFINES one of these must not define the macro, or the
	 * definition itself becomes weak.  src/pump/v90/VPcmFloModem.cpp,
	 * which now defines all four, does not.
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

	/*
	 * +0x021d was `pad_021d[1]`: `v34BaudAllow[6]` ends at the odd offset
	 * +0x21d and `bitVector` below is `short[]`, so the compiler's own
	 * 2-byte alignment inserts exactly this one byte with the member
	 * deleted -- VPCM_OFF's existing `bitVector` assertion at +0x21e
	 * (VPcmFloModem.cpp) is what proves it; confirmed zero readers/writers
	 * anywhere in the object under finding F10142, removed under F10145.
	 */

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
	 * `droppedToV34` and `clr`, which it clears in the same run of
	 * five `movb $0x0`.
	 *
	 * THIS PARAGRAPH USED TO SAY "nothing reads them here" -- true of
	 * `enterPhase3` alone and false of the class.  `runPcmModem` and
	 * `v90RunDemodulator` read and write all three: `[0]` and `[1]` are
	 * the two values `V90Jd::getConstelationSize`/`V92Jd`'s twin hand
	 * back -- a training and a data constellation size, used to tell
	 * the V.34 interface what was received (`V34XF_IndicateJdReceived`,
	 * `V34XF_IndicateDilReceived`) and to seed `setNofBitsPhase4` and the
	 * V.92 modulator's own copies (`v92modem.modulator->byte_0c`/
	 * `byte_0d`).  `[2]` is a one-shot latch: the rate-renegotiation arm
	 * that finds OUR OWN request outstanding sets it, and the arm that
	 * later sees the FAR END's report of the same event clears it and
	 * swallows the report rather than acting on it twice.
	 *
	 * `[2]` KEEPS ITS OFFSET NAME.  What is established is the latch's
	 * behaviour above, which is finding F7583; splitting the array is a
	 * separate change from fixing this comment and is not made here.
	 * `[0]`/`[1]` are left with it rather than split out on their own,
	 * since the array is cleared as one run of three by `enterPhase3`,
	 * `externalReset` and `VPcmXfCreate` and a partial split would still
	 * leave a two-purpose array under one name.
	 */
	unsigned char flags_173a[3];

	/*
	 * +0x173d  A byte `getUinfoValue` tests: non-zero skips the lookup
	 * through the modem entirely and takes the default-flags path.
	 * `enterPhase3` clears it, so entering phase 3 restores the lookup.
	 *
	 * NAMED FROM THE OBJECT'S OWN WORDS.  `runPcmModem` and
	 * `v90RunDemodulator` both set it, at the one arm each has for
	 * falling back to V.34 -- `edprintf("... drop to V34 requested
	 * !!\r\n")` immediately beside the store in each -- which is also
	 * where `v34BaudAllow` is rewritten to the fallback's own pattern
	 * (index 1 barred, index 5 allowed, neither `setV34BaudForV90`'s nor
	 * `setV34BaudForV34`'s).  So the byte is not a bare "skip the
	 * lookup" switch; it is the record that this session gave up on
	 * V.90/V.92 and dropped to V.34, and `getUinfoValue`'s short-circuit
	 * is a consequence of that rather than the byte's whole meaning.
	 */
	unsigned char droppedToV34;

	/*
	 * +0x173e  The fifth of `enterPhase3`'s run of five cleared bytes.
	 *
	 * THIS PARAGRAPH USED TO SAY "nothing else this tree has read
	 * touches it".  `runPcmModem`'s CP/CPnot/MP/MPnot/Ed arms are five
	 * calls to the free function `V90CPPacker`, whose fourth argument is
	 * "the clear flag", and this field is what four of the five pass --
	 * the fifth passes a literal 0 instead, which is the one-token
	 * difference between the otherwise-identical MP and MPnot arms.  The
	 * OBJECT NAMES IT: the MP arm's own diagnostic is
	 * `"... CP length = %d (clr=%d)\r\n"` with exactly this field as the
	 * second argument, so `clr` is the object's own abbreviation and not
	 * offset-derived.  The Ed-received arm also tests it directly to
	 * decide whether to report a cleardown (`ret = 8`) or stay silent.
	 */
	unsigned char clr;

	/*
	 * +0x173f was `pad_173f[1]`: `clr` ends at +0x173f and `sweepCounter`
	 * below is a 4-byte-aligned `int` at +0x1740, so natural alignment
	 * inserts exactly this one byte with the member deleted -- proved by
	 * adding `VPCM_OFF(sweepCounter, 0x1740, sweep)` to VPcmFloModem.cpp.
	 * Zero readers/writers anywhere in the object (F10142); removed F10145.
	 */

	/*
	 * +0x1740  THE VISUAL DIAGNOSTICS SWEEP COUNTER, and it is `int`
	 * rather than `unsigned int` because `getConstellation` DIVIDES it.
	 *
	 * The constructor zeroes it (`mov %ebp,0x1740(%ebx)` at 0xfc10, with
	 * %ebp zero) and that is all this tree could see when the field was
	 * carved out of `pad_173f`; a store of zero says a field is there and
	 * four bytes wide and nothing else.  `getConstellation` reads it now,
	 * once per point, and steps the horizontal coordinate of the trace
	 * with `counter / 15` in the data phase and `counter / 5` in phase 3.
	 *
	 * BOTH DIVISIONS ARE SIGNED, WHICH IS FORCED: 0xf43d and 0xf51d are
	 * `imul` against a reciprocal followed by `sar $0x1f` and a `sub`,
	 * which is the quotient fix-up a negative dividend needs.  An unsigned
	 * divide by 15 or by 5 is `mul` then `shr` with no fix-up at all.  So
	 * the declared type is `int` -- CLAUDE.md's forced column, finding
	 * F613's case with a division rather than a table index behind it.
	 * The field WILL go negative: it is incremented once per point
	 * forever and never reset.
	 *
	 * `sweepCounter` and not `traceX`: what it counts is calls to
	 * `getConstellation`, one per point, and the coordinate is derived
	 * from it rather than stored in it.
	 */
	int sweepCounter;				/* +0x1740         */

	/*
	 * +0x1744 .. +0x1757  THE RECEIVED MP MESSAGE, KEPT FOR THE V.34
	 * INTERFACE.  This span WAS `pad_1744`.
	 *
	 * THE WRITER NAMES IT.  `VPcmFloModem::copyMpInfoForInterface`
	 * (.text+0xd5a0, 183 bytes) is the only thing in the object that
	 * stores here, its name is the object's own out of the mangling, and
	 * its whole body is thirteen field-at-a-time copies out of
	 * `modem.mp` -- the `V90MP` embedded at +0x2428.  Every displacement
	 * lines up with a field `include/dsplib/V90MP.h` already names from
	 * `bitsToInfo`'s own diagnostics, in order and at the same widths, so
	 * the names below are the SOURCE fields' names carried across a copy
	 * rather than adjacency.
	 *
	 * AND THERE IS A READER, WHICH IS WHAT TYPES THEM.
	 * `getMPrecvdBits(tagV34Object *)` (.text+0x9250) reaches this object
	 * as `v34obj->p3548` and re-encodes the block into the V.34 side's MP
	 * word: `setne` on `mpType` for bit 0, `mpRate & 0xf` shifted to bit
	 * 6, `mpTrellis & 3` shifted to bit 11, `mpNonLin`, `mpShaping` at
	 * 0x4000 and `mpCPack` at 0x8000 -- the same six discriminators
	 * V90MP.h records, tested in the same order.  It loads the six bytes
	 * with `movsbw`/`movsbl`, which is where `char` comes from, and the
	 * seven halves with `movzwl`.
	 *
	 * ONE FIELD IS NOT A PLAIN COPY.  `mpRateMask` is
	 * `movswl 0x242e ; add %ecx,%ecx`, so it holds `mp.rateMask * 2` --
	 * the same fourteen bits one place to the left, which is the
	 * alignment `getMPrecvdBits` then ORs 0x8000 into.  The name is the
	 * source field's and the doubling is stated here rather than spelled
	 * into the name.
	 */
	char mpType;			/* +0x1744  V90MP::Type      */
	char mpRate;			/* +0x1745  V90MP::Rate      */
	char mpTrellis;			/* +0x1746  V90MP::Trellis   */
	char mpNonLin;			/* +0x1747  V90MP::NonLin    */
	char mpShaping;			/* +0x1748  V90MP::Shaping   */
	char mpCPack;			/* +0x1749  V90MP::CPack     */
	short mpRateMask;		/* +0x174a  V90MP::rateMask * 2 */
	short mpH1Real;			/* +0x174c  V90MP::h1Real    */
	short mpH1Imag;			/* +0x174e  V90MP::h1Imag    */
	short mpH2Real;			/* +0x1750  V90MP::h2Real    */
	short mpH2Imag;			/* +0x1752  V90MP::h2Imag    */
	short mpH3Real;			/* +0x1754  V90MP::h3Real    */
	short mpH3Imag;			/* +0x1756  V90MP::h3Imag    */

	/*
	 * +0x1758  The V90Modem, EMBEDDED.  See the file comment for the
	 * three independent measurements, and the .cpp for the size
	 * assertion that keeps every offset below it honest.
	 */
	V90Modem modem;

	/*
	 * +0x6118  Four bytes between the end of V90Modem's modelled prefix
	 * and the first field after it.
	 *
	 * THIS PARAGRAPH USED TO SAY "which object they belong to is not
	 * settled".  It is now: `runPcmModem` and `v90RunDemodulator`, both
	 * members of THIS class, read and write the first two on every call,
	 * so they are VPcmFloModem's own and not an overrun into V90Modem's
	 * unmeasured tail.  (`externalReset` clearing them through the
	 * VPcmFloModem is what the compiler emits either way and never did
	 * settle the question; the two entry points are what does.)
	 *
	 * `progressState` IS THE FIRST OF THE CLASS'S TWO DISPATCHES.  Both
	 * entry points switch on it before anything else, seeding the return
	 * value they otherwise only refine: 0 and 1 both mean "nothing to
	 * report yet", 2 means the session is running normally, 3 means a
	 * rate-renegotiation retrain is outstanding and becomes a report of
	 * 3 only if the receiver is in phase 4 and the parameter block's
	 * `ENABLE_ERROR_CORRECTION_RRN` allows it (arm 3 then advances the
	 * state to 4 itself), and 4 is that retrain in progress.  Every
	 * other value falls through and reports 0, which is not an error the
	 * object detects.  The two functions' case 3 differ in the ONE
	 * place documented at their own call site: `runPcmModem` takes the
	 * retrain exit when `info0Layout` is non-zero AND the other two
	 * conditions hold, `v90RunDemodulator` takes it when `info0Layout`
	 * is zero OR they do -- a single `!` that a mnemonic-only comparison
	 * of the two functions cannot see.  Usage inference, over the whole
	 * of both entry points' bodies.
	 *
	 * `retrainLatch` records that the session reached the data phase
	 * with `CFG_FLAG3_RETRAIN` freshly asserted, so that TRN1d restarting
	 * later knows to re-assert it (`v90RunDemodulator`'s own diagnostic:
	 * "ON Start TRN1d restoring SAS detector") and so that ending CPt or
	 * CPnot knows whether to put the retrain bit back.  Set once, on
	 * entering the data phase, and read by nothing that ever clears it
	 * again within this class.
	 */
	unsigned char progressState;
	unsigned char retrainLatch;

	/*
	 * +0x611a..+0x611b was `pad_611a[2]`: `retrainLatch` ends at +0x611a
	 * and `pcmSessionType` below is a 4-byte-aligned `int` at +0x611c, so
	 * natural alignment inserts exactly these two bytes with the member
	 * deleted -- the existing `VPCM_OFF(pcmSessionType, 0x611c, sesstype)`
	 * (VPcmFloModem.cpp) is what proves it. Zero readers/writers anywhere
	 * in the object (F10142); removed F10145.
	 */

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
	 * begins.  This is finding F1320's technique and V92Modem.h's own
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
	 *
	 * THEY WERE `word_6f98`, `word_6fac`, `word_6fb0` AND `word_6fb4`,
	 * and the paragraph here used to end "nothing reconstructed reads any
	 * of them, so they are offset-named".  Something does now:
	 * `qcLineVerification` is the ONLY member of this class that reads or
	 * writes any of the four, and it is all four together -- they are the
	 * whole state of the quick-connect line-verification period and of
	 * nothing else.  Its seven `dsplibs_debug_printf` messages, all
	 * prefixed `"VPcmFloModem (QC LineVerify): "`, are what name them.
	 * Finding F7603.
	 *
	 * `qcVerifyState` -- +0x6f98, three values and no more:
	 *
	 *     0  waiting for the ANSpcm demodulation to finish
	 *     1  transmitting TONEq          "...start TONEq..."
	 *     2  transmitting silence after it   "...tx silence..."
	 *
	 * and the period ends -- `qcLineVerification` returns 1, its only
	 * non-zero return -- out of state 2 with "Silence after TONEq over,
	 * move to phase2...".  The value set is the object's; the word
	 * "state" is inference over three arms and is labelled as such.
	 *
	 * `qcSampleCount` -- +0x6fac, AND IT IS `int`.  Forced twice over:
	 * `jle` at 0xf8a2 and 0xf931 against 0x1df, and `js` at 0xf96c, where
	 * an unsigned count would be `jbe` and could not be negative at all.
	 * It has to be signed because the silence period is spelled as a
	 * NEGATIVE count -- 0xfffffe80, -384 -- that counts up to zero.  The
	 * function's own messages print it with `%d` and call it "samples".
	 *
	 * `qcTerminateRequested` -- +0x6fb0, a 0/1 latch.  The TONEq ends when
	 * this is set AND 480 samples have gone by; the message at the site
	 * that sets it is "TONEq termination requested, still bellow 50mS",
	 * which is also where the two constants come from.  480 samples is
	 * 50 ms at the 9600 Hz this class's `SineWave` is built for, and the
	 * -384 above is 40 ms at the same rate.
	 *
	 * `verificationStatus` -- +0x6fb4, a COPY of
	 * `V90Phase3Demodulator::verificationStatus`, which this tree already
	 * names that.  It is taken with a `movzwl`, so the SOURCE is sixteen
	 * bits wide even though the field it comes out of is declared 32
	 * (V90Phase3Demodulator.h) and the word here is 32; the 32-bit result
	 * is stored, so this is CLAUDE.md's forced column and not 614's free
	 * one.  `v34pcmmain.cpp` is the reader: it compares `local_short`
	 * against this word to decide "short phase2 due to same line
	 * verification".
	 */
	unsigned int qcVerifyState;			/* +0x6f98         */

	/*
	 * +0x6f9c  A SineWave<float, float>, EMBEDDED and 16 bytes -- four
	 * `Tparam`s, and `Tparam` is `float` here.  The constructor builds it
	 * with (4800.0f, 980.0f, 0.0f, 9600.0f) and both destructors run
	 * `_ZN8SineWaveIffED1Ev` on `this + 0x6f9c`.  0x6f9c + 0x10 is
	 * 0x6fac, the next field, which is finding F1320's bound again and
	 * agrees with SineWave.h's own four-field map.
	 */
	SineWave<float, float> sineWave;

	int qcSampleCount;				/* +0x6fac         */
	unsigned int qcTerminateRequested;		/* +0x6fb0         */
	unsigned int verificationStatus;		/* +0x6fb4         */

	/*
	 * +0x6fb8  FOUR BYTES, AND NOT EXPLAINED BY ALIGNMENT.  `cpBitVector`
	 * below is a `short` array and needs only 2-byte alignment, and
	 * +0x6fb8 is already 4-byte aligned, so a plain field-to-field gap
	 * would be 0 bytes here, not 4 -- unlike every other `pad_NNNN` in
	 * this class, which is each exactly as wide as the next field's own
	 * alignment demands (verified the same way below).
	 *
	 * CHECKED AND STILL PAD.  A `this`-relative-displacement search of
	 * every VPcmFloModem member function (`dis.py` over 0xd030..0x1016b,
	 * which covers all of them) finds no instruction touching
	 * +0x6fb8..+0x6fbb, and neither does a search of the whole 1.2 MB
	 * object (`objdump -d` grepped for the literal displacement).  Same
	 * shape as `cadence`'s `pad_2c0` (F10137): zero readers AND zero
	 * writers anywhere in the blob, which is the strongest evidence this
	 * phase can have that space is genuinely unmodelled rather than
	 * merely unread by what we happen to have reconstructed -- the
	 * reconstruction is complete, so "nothing touches it" is a fact about
	 * the object, not a gap in our closure.  Left as one span rather than
	 * guessed into fields.  Finding F10142.
	 */
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

	/*
	 * +0x7dd3 was `pad_7dd3[1]`: `nofBitsPerSymbol` ends at +0x7dd3 and
	 * `nofTransmitSequences` below is a 2-byte-aligned `unsigned short` at
	 * +0x7dd4, so natural alignment inserts exactly this one byte with the
	 * member deleted -- the existing `VPCM_OFF(nofTransmitSequences,
	 * 0x7dd4, nseq)` (VPcmFloModem.cpp) is what proves it. Zero
	 * readers/writers anywhere in the object (F10142); removed F10145.
	 */

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
	 * bytes of alignment fill the object exactly.
	 *
	 * `byte_7f5c` STAYS OFFSET-NAMED: nothing this tree has read touches
	 * it beyond the constructor's clear.
	 *
	 * `ecMode` AND `ecRampCounter` ARE NOT: `runPcmModem` reads and
	 * writes both, and `VPCM_EC_RAMP_MODE`/`_START`/`_SENTINEL` below are
	 * the object's own three constants for them.  `ecMode` selects
	 * whether the echo canceller's input this block is the real
	 * receive sample (any other value) or a synthetic ramp
	 * (`VPCM_EC_RAMP_MODE`, entered when the demodulator reports
	 * rate-renegotiation silence); `ecRampCounter` is the ramp's own
	 * position, reset to `VPCM_EC_RAMP_START` on every mode change and
	 * stepped by one per block while the ramp runs, capped at
	 * `VPCM_EC_RAMP_SENTINEL` -- which `V92EchoCanceller::process` is
	 * documented (at the ramp's definition, below) to treat as "pass the
	 * input through untouched".  Usage inference: the object never
	 * prints either field's name.
	 */
	unsigned char byte_7f5c;			/* +0x7f5c         */

	/*
	 * +0x7f5d..+0x7f5f was `pad_7f5d[3]`: `byte_7f5c` ends at +0x7f5d and
	 * `ecMode` below is a 4-byte-aligned `unsigned int` at +0x7f60, so
	 * natural alignment inserts exactly these three bytes with the member
	 * deleted -- proved by adding `VPCM_OFF(ecMode, 0x7f60, ecmode)` to
	 * VPcmFloModem.cpp. Zero readers/writers anywhere in the object
	 * (F10142); removed F10145.
	 */
	unsigned int ecMode;				/* +0x7f60         */
	unsigned int ecRampCounter;			/* +0x7f64         */
};

#endif /* DSPLIB_VPCMFLOMODEM_H */

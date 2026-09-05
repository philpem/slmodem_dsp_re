/**
 * @file VPcmFloModem.h
 * @brief The V.90/V.92 modem's face to the V.34 handshake.
 *
 * The class has twenty-six members in the blob. This tree writes six out of
 * line (`getUinfoValue`, `setPhaseIIinfo`, `getV90CpBits`, `getV90JaBits`,
 * `setPcmSessionType`, `enterPhase3`), plus `externalReset`, the two
 * `setV34BaudFor*`, the three visual diagnostics, and both entry points
 * (`runPcmModem`, `v90RunDemodulator`). Seven more are reconstructed as
 * `inline` bodies that the two entry points inline exactly as the object
 * does (see the block that lists them below). Everything else is left
 * undeclared rather than declared-and-undefined, because nothing here calls
 * it and a declaration nobody needs is a claim nobody checked.
 *
 * Not polymorphic: `tools/cppstruct.py` lists the destructor with `D1`/`D2`
 * and no `D0`, and GCC emits a deleting destructor only for a virtual one,
 * so +0x00 is a real member and there is no vptr (finding F228 lists the
 * four classes where that is not true).
 *
 * @par The object really is thirty-two kilobytes
 * `docs/v90cpp.md` used to read this class's +32,612 reach as finding
 * F268's warning sign -- that a displacement this large usually means
 * indexing *through* `this` into an enclosing session object rather than
 * the class really being that big. Tracing the base register of the five
 * largest displacements (in `getV90JaBits`, `getV90CpBits`,
 * `setPcmSessionType`, `setPhaseIIinfo`, `getUinfoValue`) back to the
 * prologue finds no intervening load in any of them: all five come
 * straight off the `this` stack slot. So the floor really is
 * 0x7f28 = 32,552 bytes (where the last of the four float arrays ends),
 * with the object's full 32,612-byte reach 60 bytes further on. A floor is
 * still not a size, so none is asserted here or in the .cpp: twenty-one of
 * the twenty-six members are unread, and the four arrays at +0x7dd8 are
 * only known to be `VPCM_L2` entries because that's how many
 * `getUinfoValue` clears. The .cpp asserts offsets; the test allocates a
 * slot larger than the floor and compares the whole slot, catching a store
 * past the last modelled field as well as one inside it.
 *
 * @par A V90Modem is embedded at +0x1758
 * Measured three independent ways: `setPcmSessionType` reaches it with an
 * add-then-jump into `V90Modem::setSessionFlag` (the same forwarding
 * signature finding F268 used for the two demodulators); `getV90CpBits`
 * reads +0x175c as `V90Modem::demodulator` (its own +0x04); and
 * `setPhaseIIinfo` reads +0x1760 as `V90Phase2Info*`, landing exactly at
 * `V90Modem`'s declared prefix end (0x1758 + 0x49c0 = 0x6118), immediately
 * before the next field anything here touches, +0x611c. The third leg is
 * the weak one -- `V90Modem` asserts no size of its own -- so what holds
 * the map together is the .cpp's `sizeof(V90Modem) == 0x49c0` assertion
 * alongside the offsets: a later batch that gives `V90Modem` more prefix
 * breaks the build here rather than silently shifting every offset past
 * +0x6118. Two fields inside that embedded `V90Modem` were carved out of
 * its `pad_08` for this batch: `phase2Info` at +0x08 (this + 0x1760) and
 * `ptr_49b4` at +0x49b4 (this + 0x610c) -- see
 * `include/dsplib/V90SessionFlag.h`.
 *
 * @par Names
 * The mangling never carries a data member's name (finding F226); where
 * the blob names a field some other way, that name is used below. Four
 * examples: `setTerminateJaFlag`/`CpFlag`/`CpNotFlag` and
 * `getV90CpBits`'s own `"(terminateCp=%d, terminateCpNot=%d)"` diagnostic
 * (printed in that order) between them place the Ja/Cp/CpNot flags at
 * +0x7dce/+0x7dcf/+0x7dd0; `setMinNofTransmitSequences(unsigned short)`
 * types the field it's compared against at +0x7dd6; `"End of CP #%d
 * tx...."` names +0x7dd4; and `L2` at +0x7e80 comes from
 * `setPhaseIIinfo` installing it in `V90Phase2Info`'s `L2` slot and
 * `getUinfoValue` printing it as `"L2[%d]"`. Everything else is
 * offset-named or descriptive-and-hedged, and each field comment says
 * which.
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

/** @brief Entries in each of the four float arrays getUinfoValue() clears
 *  (`inc %edx; cmp $0x14,%edx; jle` is 0..20 inclusive) -- the same bound
 *  `V90PHASE2INFO_L2` and `V92PHASE2INFO_L2` record, from two other
 *  translation units' printers. */
#define VPCM_L2			21

/** @brief Probe tones getUinfoValue() walks (`cmp $0x18,%edx; jle` is
 *  0..24), matching `V34_PROBE_RESULTS` in dsplib/v34fsk.h. */
#define VPCM_PROBE_TONES	25

class VPcmFloModem {
public:
	/**
	 * @brief Construct the whole modem: six member objects and the
	 *        wiring between them. See src/pump/v90/VPcmFloModemCtor.cpp,
	 *        which accounts for every argument passed on.
	 * @param v34Object    The owning V.34 object, stored and handed back
	 *                     to `V34XF_Get*` calls verbatim.
	 * @param side         Call or answer.
	 * @param modemParams  Parameters shared with V.34/V.90/V.92.
	 * @param nSamples     Block size.
	 * @param v90Mode      V.90 computational mode.
	 * @param v92Mode      V.92 computational mode.
	 */
	VPcmFloModem(void *v34Object, V90ModemSide side,
		     _tagModemParameters *modemParams, unsigned int nSamples,
		     V90ComputationalMode v90Mode,
		     V92ComputationalMode v92Mode);

	/**
	 * @brief Destroy. No declared body of its own -- the blob's `D1`/`D2`
	 *        are six member destructor calls in reverse declaration
	 *        order and nothing else, exactly what GCC emits for an
	 *        implicitly-declared destructor over these six members.
	 *        Declaring an empty body here would not be the same
	 *        function under `-fno-lifetime-dse`, so none is declared;
	 *        deviation D237 covers the resulting symbol gap.
	 */
	~VPcmFloModem();

	/**
	 * @brief Reset the transmit-side bookkeeping to its phase-3 entry
	 *        values, with the V.90 baud allow list applied (index 5
	 *        barred).
	 */
	void internalReset();

	/**
	 * @brief Turn the V.34 line probe into the Phase 2 record, and
	 *        report whatever the modem already knows about Uinfo.
	 * @param probeValid  Nonzero when the V.34 probe result is usable.
	 * @return 0 unless a non-zero short is found through the modem.
	 */
	int getUinfoValue(short probeValid);

	/**
	 * @brief Fill both Phase 2 records from the 41 INFO0 bits, install
	 *        the four float arrays in them, and re-apply the PCM
	 *        session type.
	 * @param info0  The 41 INFO0 bits (mutable: `int *`, not `const
	 *               int *` -- the mangling is `Pi`, not `PKi`).
	 * @param rtd    Round-trip delay, stored into both records.
	 */
	void setPhaseIIinfo(int *info0, int rtd);

	/**
	 * @brief Pack the next CP symbol.
	 * @param bits  Destination for the packed symbol.
	 * @return 1 once CP is to be terminated, 0 otherwise.
	 */
	int getV90CpBits(short *bits);

	/**
	 * @brief Pack the next JA symbol.
	 * @param bits  Destination for the packed symbol.
	 * @return 1 once JA is to be terminated, 0 otherwise.
	 */
	int getV90JaBits(short *bits);

	/**
	 * @brief Record the session type and tell the embedded modem.
	 * @param sessionType  0 for V.90, non-zero for V.92.
	 */
	void setPcmSessionType(int sessionType);

	/**
	 * @brief Clear the transmit bookkeeping, hand phase 3 to the
	 *        demodulator, and pack the DIL descriptor into `bitVector`
	 *        as the JA vector.
	 */
	void enterPhase3();

	/**
	 * @brief Put a constructed modem back to its starting state.
	 *        `VPcmV34Create`'s way of resetting; added by task #88.
	 *        Nearly `enterPhase3`'s shape: the same six flags, the same
	 *        five cleared bytes, the same three CP fields.
	 */
	void externalReset();

	/** @brief Set `v34BaudAllow` for a V.90 session (index 5, the
	 *  fastest V.34 rate, barred). Not called by anything in the
	 *  object; survives only as the out-of-line copy, like
	 *  `setScramble` in v34shell.h. */
	void setV34BaudForV90();
	/** @brief Set `v34BaudAllow` for a V.34 session (index 5 allowed,
	 *  the only difference from setV34BaudForV90()). Likewise
	 *  uncalled; out-of-line copy only. */
	void setV34BaudForV34();

	/*
	 * --- Seven members v90RunDemodulator inlines -------------------------
	 *
	 * Each is a `T` symbol of its own in the blob with no incoming
	 * relocation anywhere in the object, and each one's body appears
	 * open-coded inside `v90RunDemodulator` -- what a call the compiler
	 * inlined looks like, since the out-of-line copy still has to be
	 * emitted for a non-inline member whether anything reaches it or not.
	 * The `inline` keyword has been dropped and all seven symbols are
	 * claimed (`t_vpcmleaves.cpp` is their tests); the fourteen inlined
	 * sites in `v90RunDemodulator` are unaffected, since GCC still inlines
	 * a same-TU callee at -O3.
	 */
	/** @brief Set the Ja termination-request flag (`terminateJa`, +0x7dce). */
	void setTerminateJaFlag(unsigned char v);
	/** @brief Set the CP termination-request flag (`terminateCp`, +0x7dcf). */
	void setTerminateCpFlag(unsigned char v);
	/** @brief Set the CPnot termination-request flag (`terminateCpNot`, +0x7dd0). */
	void setTerminateCpNotFlag(unsigned char v);
	/** @brief Set the minimum completed-sequence count before CP may give
	 *  way to CPnot (`minNofTransmitSequences`, +0x7dd6). */
	void setMinNofTransmitSequences(unsigned short n);
	/** @brief Set how many bits getV90CpBits() packs per output word
	 *  (`nofBitsPerSymbol`, +0x7dd2). */
	void setNofBitsPhase4(unsigned int constel);
	/** @brief Rewind the transmit bit pointer to the start of `bitVector`
	 *  (`bitPointer`, +0x1738). */
	void resetBitPointer();
	/** @brief Copy the received MP message from the embedded V90Modem's
	 *  `V90MP` into this object's own `mpType`..`mpH3Imag` block, so the
	 *  V.34 interface can read it without reaching into the V.90 modem. */
	void copyMpInfoForInterface();

	/*
	 * --- The three visual diagnostics -------------------------------------
	 *
	 * `VPcmV34GetVisualDiagnostics` dispatches to these three for a PCM
	 * session (src/pump/v34/v34diag.cpp; include/dsplib/int_complex.h for
	 * what a point is). Argument types are the mangling's exactly
	 * (`P11int_complexm` is `(int_complex *, unsigned long)`); return
	 * types are not mangled, but all three leave the point count written
	 * in %eax on every path (zero on the paths that write none), so
	 * `unsigned long` -- the same width/signedness as the clamp bound --
	 * is what the object supports. All three answer nothing unless a PCM
	 * receiver is running: each opens with the equivalent of
	 * `if (pcmSessionType != 0 && info0Layout == 0) return 0`.
	 */
	/** @brief Fetch constellation trace points for the visual diagnostics.
	 *  @param points    Destination array.
	 *  @param maxCount  Capacity of `points`.
	 *  @return Number of points written. */
	unsigned long getConstellation(int_complex *points,
				       unsigned long maxCount);
	/** @brief Fetch linear-equalizer trace points for the visual diagnostics.
	 *  @param points    Destination array.
	 *  @param maxCount  Capacity of `points`.
	 *  @return Number of points written. */
	unsigned long getLinearEqualizer(int_complex *points,
					 unsigned long maxCount);
	/** @brief Fetch DFE trace points for the visual diagnostics.
	 *  @param points    Destination array.
	 *  @param maxCount  Capacity of `points`.
	 *  @return Number of points written. */
	unsigned long getDFE(int_complex *points, unsigned long maxCount);

	/*
	 * --- The four VPcmV34Progress entry points ----------------------------
	 *
	 * All four are written, in src/pump/v90/VPcmFloModem.cpp.
	 * `VPcmV34Progress` (src/pump/v34/v34pcmmain.cpp) calls all four and
	 * nothing else does. Each is declared weak via
	 * `DSPLIB_VPCMFLO_UNWRITTEN`, defined empty just below: a weak
	 * DECLARATION whose symbol is defined at link time resolves to the
	 * definition, so `v34pcmmain.cpp`'s guard passes at every call site
	 * and the calls happen unconditionally, matching the blob. The macro
	 * stays rather than being removed because `v34pcmmain.cpp` has to
	 * keep working if a future split ever takes a member back out;
	 * `include/dsplib/vpcm.h` carries the same arrangement for the five
	 * `VPcmV34*` entry points and explains it at length. A TU that
	 * DEFINES one of these must not itself define the macro, or the
	 * definition becomes weak too -- `VPcmFloModem.cpp`, which defines
	 * all four, does not. Return types are not mangled: `int` is what
	 * `VPcmV34Progress` switches on for the first three, and
	 * `vPcmResetPhase3Modem`'s result is discarded, so it is `void`.
	 */
#ifndef DSPLIB_VPCMFLO_UNWRITTEN
#define DSPLIB_VPCMFLO_UNWRITTEN
#endif
	/**
	 * @brief Run one block of the full V.90/V.92 PCM modem: transmit and
	 *        receive together. `VPcmV34Progress`'s main entry point.
	 * @param in      Received samples.
	 * @param out     Transmit samples to send out.
	 * @param n       Block size, in samples.
	 * @param rxbits  Received bits, if any completed this block.
	 * @param nrx     Number of bits written to `rxbits`.
	 * @param txbits  Transmit bits to encode into `out`.
	 * @param nbits   Number of bits in `txbits`.
	 * @return The progress code `VPcmV34Progress` switches on.
	 */
	int runPcmModem(float *in, float *out, unsigned int n, int *rxbits,
			int *nrx, int *txbits, int *nbits)
		DSPLIB_VPCMFLO_UNWRITTEN;
	/**
	 * @brief Run one block of the V.90/V.92 demodulator alone (no
	 *        transmit side), used while the receive-only phases run.
	 * @param in      Received samples.
	 * @param n       Block size, in samples.
	 * @param rxbits  Received bits, if any completed this block.
	 * @param nrx     Number of bits written to `rxbits`.
	 * @return The progress code `VPcmV34Progress` switches on.
	 */
	int v90RunDemodulator(float *in, unsigned int n, int *rxbits, int *nrx)
		DSPLIB_VPCMFLO_UNWRITTEN;
	/**
	 * @brief Run one block of the quick-connect line-verification period
	 *        (see `qcVerifyState` and friends).
	 * @param in      Received samples.
	 * @param out     Transmit samples to send out.
	 * @param n       Block size, in samples.
	 * @param rxbits  Received bits, if any completed this block.
	 * @param nrx     Number of bits written to `rxbits`.
	 * @param txbits  Transmit bits to encode into `out`.
	 * @param nbits   Number of bits in `txbits`.
	 * @return 1 once line verification's period ends, 0 otherwise.
	 */
	int qcLineVerification(float *in, float *out, unsigned int n,
			       int *rxbits, int *nrx, int *txbits, int *nbits)
		DSPLIB_VPCMFLO_UNWRITTEN;
	/** @brief Reset the modem back to its phase-3 entry state. */
	void vPcmResetPhase3Modem() DSPLIB_VPCMFLO_UNWRITTEN;

	/* --- data members; see the file comment on the naming --- */

	/* +0x0000  The V.34 object, handed to the constructor as its `void *`
	 * first argument and passed straight back to `V34XF_GetRTD`,
	 * `V34XF_GetInfo0BitsPtr` and `V34XF_GetProbeResultsPtr`, all three of
	 * which take a `void *` (include/dsplib/v34pcmif.h). */
	void *v34Object;

	/* +0x0004  The DIL descriptor, embedded, and the JA vector's source
	 * (enterPhase3() packs it here). An add off `this`, not a load, so
	 * the block is in the object; typed by `DILdescriptorPacker` taking
	 * a `const tagV90DILdescriptor *` and `sizeof(tagV90DILdescriptor)`
	 * == 0x213 running exactly to the next measured field, +0x217. */
	tagV90DILdescriptor dil;

	/*
	 * +0x0217  Which of V.34's six symbol rates this session will
	 * accept, one byte each, 1 for allowed. Named (not offset-named) on
	 * three independent readings: `chkForceBaudRate` (v34pcmif.c) indexes
	 * `p3548 + 0x217` 1..5 as `sel` against a cap it calls "max V34 baud
	 * rate index", and in its other arms indexes a local
	 * `unsigned char allow[6]` the same way; `setV34BaudForV90`/
	 * `setV34BaudForV34` are six stores to this array and nothing else,
	 * differing only in the last (fastest, 3429 baud) entry -- which
	 * `enterPhase3`/`externalReset` bar and `getUinfoValue` allows. The
	 * index-to-rate mapping itself is not established (only the
	 * ascending bar order is): the name claims the array is per-baud,
	 * not which baud each index is.
	 */
	unsigned char v34BaudAllow[6];

	/*
	 * +0x021d was `pad_021d[1]`: `v34BaudAllow[6]` ends at the odd offset
	 * +0x21d and `bitVector` below is `short[]`, so the compiler's own
	 * 2-byte alignment inserts exactly this one byte with the member
	 * deleted -- VPCM_OFF's existing `bitVector` assertion at +0x21e
	 * (VPcmFloModem.cpp) is what proves it; confirmed zero readers/writers
	 * anywhere in the object under finding F10142, removed under F10150.
	 */

	/*
	 * +0x021e  The bit vector currently being transmitted, indexed by
	 * `bitPointer` and `nofBits` long. `getV90JaBits` reads it two bits
	 * at a time; `getV90CpBits` copies `nofBits` of it into
	 * `cpBitVector` when CP gives way to CPnot.
	 *
	 * The declared length is a span, not the original's bound: nothing
	 * read here bounds the array (it's indexed by `nofBits`, which these
	 * five methods only read), so the declaration runs to the next
	 * offset that IS measured -- a store anywhere in the span lands
	 * inside a modelled field rather than off the end of one.
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
	 * +0x173a  Three bytes `enterPhase3` clears in the same run as
	 * `droppedToV34` and `clr`, and reads/writes for two different
	 * purposes: `[0]`/`[1]` are the training/data constellation sizes
	 * `V90Jd::getConstelationSize`/`V92Jd`'s twin hand back, used to tell
	 * the V.34 interface what was received (`V34XF_IndicateJdReceived`,
	 * `V34XF_IndicateDilReceived`) and to seed `setNofBitsPhase4` and the
	 * V.92 modulator's own copies (`v92modem.modulator->byte_0c`/
	 * `byte_0d`); `[2]` is a one-shot latch -- set when the
	 * rate-renegotiation arm finds our own request outstanding, cleared
	 * (swallowing the report rather than acting on it twice) when the far
	 * end's report of the same event arrives later (finding F7583).
	 * `[2]` keeps its offset name; `[0]`/`[1]` stay with it rather than
	 * splitting out, since all three are cleared together by
	 * `enterPhase3`, `externalReset` and `VPcmXfCreate`.
	 */
	unsigned char flags_173a[3];

	/*
	 * +0x173d  Non-zero skips the Uinfo lookup through the modem
	 * entirely and takes the default-flags path in getUinfoValue();
	 * enterPhase3() clears it, restoring the lookup. Named from the
	 * object's own words: `runPcmModem` and `v90RunDemodulator` each set
	 * it at their one V.34-fallback arm, right beside
	 * `edprintf("... drop to V34 requested !!\r\n")`, which is also
	 * where `v34BaudAllow` is rewritten to the fallback's own pattern
	 * (index 1 barred, index 5 allowed -- neither `setV34BaudForV90`'s
	 * nor `setV34BaudForV34`'s). So the byte records that this session
	 * gave up on V.90/V.92 and dropped to V.34; `getUinfoValue`'s
	 * short-circuit is a consequence of that, not the byte's whole
	 * meaning.
	 */
	unsigned char droppedToV34;

	/*
	 * +0x173e  The fifth of enterPhase3()'s run of five cleared bytes.
	 * `runPcmModem`'s CP/CPnot/MP/MPnot/Ed arms are five calls to the
	 * free function `V90CPPacker`, whose fourth argument ("the clear
	 * flag") is what four of the five pass -- the fifth passes a literal
	 * 0, the one-token difference between the otherwise-identical MP and
	 * MPnot arms. The object names it: the MP arm's own diagnostic is
	 * `"... CP length = %d (clr=%d)\r\n"` with this field as the second
	 * argument. The Ed-received arm also tests it directly to decide
	 * whether to report a cleardown (`ret = 8`) or stay silent.
	 */
	unsigned char clr;

	/*
	 * +0x173f was `pad_173f[1]`: `clr` ends at +0x173f and `sweepCounter`
	 * below is a 4-byte-aligned `int` at +0x1740, so natural alignment
	 * inserts exactly this one byte with the member deleted -- proved by
	 * adding `VPCM_OFF(sweepCounter, 0x1740, sweep)` to VPcmFloModem.cpp.
	 * Zero readers/writers anywhere in the object (F10142); removed F10150.
	 */

	/*
	 * +0x1740  The visual diagnostics sweep counter -- `sweepCounter`,
	 * not `traceX`, because what it counts is calls to
	 * `getConstellation`, one per point, and the trace coordinate is
	 * derived from it rather than stored in it. `int` rather than
	 * `unsigned int` is forced: `getConstellation` divides it by 15 (data
	 * phase) or 5 (phase 3) with the `imul`-by-reciprocal-plus-`sar`
	 * fix-up a signed division needs, where an unsigned divide would be a
	 * plain `mul`/`shr` (CLAUDE.md's forced column, finding F613's case).
	 * It is incremented once per point forever and never reset, so it
	 * will go negative.
	 */
	int sweepCounter;				/* +0x1740         */

	/*
	 * +0x1744 .. +0x1757  The received MP message, kept for the V.34
	 * interface. The writer names it: `copyMpInfoForInterface`
	 * (.text+0xd5a0) is the only thing that stores here, and its body is
	 * thirteen field-at-a-time copies out of `modem.mp` (the embedded
	 * `V90MP` at +0x2428) at matching displacements and widths, so the
	 * names below are `V90MP.h`'s own source-field names carried across
	 * the copy. The reader types them: `getMPrecvdBits(tagV34Object *)`
	 * re-encodes this block into the V.34 side's MP word using the same
	 * six discriminators `V90MP.h` records, in the same order, loading
	 * the six bytes with `movsbw`/`movsbl` (hence `char`) and the seven
	 * halves with `movzwl`. One field is not a plain copy: `mpRateMask`
	 * holds `mp.rateMask * 2` (`movswl` then `add %ecx,%ecx`) -- the same
	 * fourteen bits shifted one place, which is the alignment
	 * `getMPrecvdBits` then ORs 0x8000 into.
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

	/* +0x1758  The V90Modem, embedded. See the file comment for the three
	 * independent measurements, and the .cpp for the size assertion that
	 * keeps every offset below it honest. */
	V90Modem modem;

	/*
	 * +0x6118  Two bytes belonging to VPcmFloModem, not an overrun into
	 * V90Modem's unmeasured tail: `runPcmModem` and `v90RunDemodulator`,
	 * both members of this class, read and write both on every call.
	 *
	 * `progressState` is the first of the class's two dispatches. Both
	 * entry points switch on it before anything else, seeding the return
	 * value they otherwise only refine: 0 and 1 both mean "nothing to
	 * report yet", 2 means the session is running normally, 3 means a
	 * rate-renegotiation retrain is outstanding and becomes a report of
	 * 3 only if the receiver is in phase 4 and the parameter block's
	 * `ENABLE_ERROR_CORRECTION_RRN` allows it (arm 3 then advances the
	 * state to 4 itself), and 4 is that retrain in progress. Every other
	 * value falls through and reports 0, which the object does not treat
	 * as an error. The two functions' case 3 differ in one place:
	 * `runPcmModem` takes the retrain exit when `info0Layout` is
	 * non-zero AND the other two conditions hold, `v90RunDemodulator`
	 * takes it when `info0Layout` is zero OR they do -- a single `!`
	 * that a mnemonic-only comparison of the two functions cannot see.
	 *
	 * `retrainLatch` records that the session reached the data phase
	 * with `CFG_FLAG3_RETRAIN` freshly asserted, so that TRN1d restarting
	 * later knows to re-assert it (`v90RunDemodulator`'s own diagnostic,
	 * "ON Start TRN1d restoring SAS detector") and so that ending CPt or
	 * CPnot knows whether to put the retrain bit back. Set once, on
	 * entering the data phase; nothing in this class ever clears it
	 * again.
	 */
	unsigned char progressState;
	unsigned char retrainLatch;

	/*
	 * +0x611a..+0x611b was `pad_611a[2]`: `retrainLatch` ends at +0x611a
	 * and `pcmSessionType` below is a 4-byte-aligned `int` at +0x611c, so
	 * natural alignment inserts exactly these two bytes with the member
	 * deleted -- the existing `VPCM_OFF(pcmSessionType, 0x611c, sesstype)`
	 * (VPcmFloModem.cpp) is what proves it. Zero readers/writers anywhere
	 * in the object (F10142); removed F10150.
	 */

	/* +0x611c  V.90 or V.92, as a 0/1 int: `setPcmSessionType` stores
	 * `(arg != 0)` here and `setPhaseIIinfo` reads it back to re-apply
	 * it. Named from both printing `"setting PCM session to V.%d"` with
	 * 92 for non-zero and 90 for zero. */
	int pcmSessionType;

	/* +0x6120  Which layout `setPhaseIIinfo` reads the INFO0 bits in. It
	 * gates two things only: whether INFO0 bits 38/39 are stored as
	 * `txPowerMeasurementPoint`/`pcmType` at all, and whether INFO0 bit
	 * 26 or bit 27 is the "short phase 2 remote" byte. Descriptive and
	 * hedged -- the object never names it. */
	int info0Layout;

	/*
	 * +0x6124  The V92Modem, embedded, 0xaac bytes: the constructor
	 * builds it with an add off `this` (not a load), both destructors
	 * destroy it there, and `sizeof(V92Modem)` == 0xaac agrees exactly
	 * with the gap to the next member at +0x6bd0 (finding F1320's
	 * technique). This span used to be modelled as two loose pointers,
	 * `v92Params` and `v92Phase2Info`, plus padding either side; both
	 * were correct and sit at exactly the offsets recorded
	 * (`V92Modem::parameters` at the V92Modem's own +0x004, `phase2Info`
	 * at +0x008) -- what changed is that they're now reached through the
	 * member that owns them.
	 */
	V92Modem v92modem;

	/* +0x6bd0  The V92EchoCanceller, embedded, 0x3c bytes; built with the
	 * V92Modem's `parameters` and destroyed by both destructors at this
	 * address. `sizeof(V92EchoCanceller)` == 0x3c agrees. */
	V92EchoCanceller echoCanceller;

	/* +0x6c0c  848 bytes the constructor clears (`sysdep_memset(p, 0,
	 * 0x350)`) and nothing else in this tree touches. Not part of the
	 * echo canceller: bounded on both sides by objects whose sizes are
	 * asserted elsewhere (0x6c0c is one past `V92EchoCanceller`'s last
	 * byte, 0x6c0c + 0x350 is exactly where `ANSamToneDetector` begins),
	 * so the span belongs to this class. Offset-named: a memset says how
	 * big a thing is and nothing about what it holds. */
	unsigned char block_6c0c[0x6f5c - 0x6c0c];	/* +0x6c0c         */

	/* +0x6f5c  The ANSamToneDetector, embedded, 0x3c bytes
	 * (src/pump/v90/ANSamToneDetector.cpp asserts it); 0x6f5c + 0x3c is
	 * 0x6f98, the next field, so the two bound each other. */
	ANSamToneDetector ansam;

	/*
	 * +0x6f98, +0x6fac, +0x6fb0, +0x6fb4  Four words cleared by
	 * `externalReset` and the constructor, and the only four things
	 * `qcLineVerification` touches between the V.92 modem and the CP bit
	 * vector -- the whole state of the quick-connect line-verification
	 * period and nothing else, named by its seven
	 * `"VPcmFloModem (QC LineVerify): "`-prefixed diagnostics (finding
	 * F7603).
	 *
	 * `qcVerifyState` (+0x6f98) takes three values: 0 waiting for the
	 * ANSpcm demodulation to finish, 1 transmitting TONEq, 2 transmitting
	 * silence after it; the period ends (the function's only non-zero
	 * return) out of state 2. "State" is inference over the three arms.
	 *
	 * `qcSampleCount` (+0x6fac) is `int`, forced twice over (`jle`
	 * against 0x1df, `js`, where an unsigned count would be `jbe` and
	 * could never go negative): the silence period is spelled as a
	 * negative count, -384, counting up to zero. The messages print it
	 * with `%d` and call it "samples".
	 *
	 * `qcTerminateRequested` (+0x6fb0) is a 0/1 latch; TONEq ends when
	 * it's set and 480 samples have gone by. 480 samples is 50 ms at the
	 * 9600 Hz this class's `SineWave` runs at, and the -384 above is
	 * 40 ms at the same rate.
	 *
	 * `verificationStatus` (+0x6fb4) is a copy of
	 * `V90Phase3Demodulator::verificationStatus`, taken with a `movzwl`
	 * (source sixteen bits wide, stored 32-bit result -- CLAUDE.md's
	 * forced column). `v34pcmmain.cpp` compares it against `local_short`
	 * to decide "short phase2 due to same line verification".
	 */
	unsigned int qcVerifyState;			/* +0x6f98         */

	/* +0x6f9c  A SineWave<float, float>, embedded, 16 bytes (four
	 * `float` `Tparam`s), built with (4800.0f, 980.0f, 0.0f, 9600.0f).
	 * 0x6f9c + 0x10 is 0x6fac, the next field (finding F1320's bound
	 * again), agreeing with SineWave.h's own four-field map. */
	SineWave<float, float> sineWave;

	int qcSampleCount;				/* +0x6fac         */
	unsigned int qcTerminateRequested;		/* +0x6fb0         */
	unsigned int verificationStatus;		/* +0x6fb4         */

	/*
	 * +0x6fb8  Four bytes, not explained by alignment: `cpBitVector`
	 * below is a `short` array needing only 2-byte alignment, and
	 * +0x6fb8 is already 4-byte aligned, so a plain field-to-field gap
	 * would be 0 bytes here, not 4 -- unlike every other `pad_NNNN` in
	 * this class, which is each exactly as wide as the next field's own
	 * alignment demands.
	 *
	 * Checked and still pad: a `this`-relative-displacement search of
	 * every VPcmFloModem member function, and of the whole 1.2 MB
	 * object, finds no instruction touching +0x6fb8..+0x6fbb. Same shape
	 * as `cadence`'s `pad_2c0` (F10137) -- zero readers and zero writers
	 * anywhere in the blob, the strongest evidence this phase can have
	 * that the space is genuinely unmodelled rather than merely unread
	 * by what's been reconstructed so far, since the reconstruction here
	 * is complete. Left as one span rather than guessed into fields
	 * (finding F10142).
	 */
	unsigned char pad_6fb8[0x6fbc - 0x6fb8];	/* +0x6fb8         */

	/* +0x6fbc  The CP bit vector, `cpNofBits` long. Filled by
	 * `getV90CpBits` from `bitVector` at the CP-to-CPnot transition and
	 * read by it on every call. A span, like `bitVector` above. */
	short cpBitVector[(0x7dcc - 0x6fbc) / 2];

	/* +0x7dcc  How many entries of `cpBitVector` are live. `movswl`. */
	short cpNofBits;

	/* +0x7dce, +0x7dcf, +0x7dd0  The three termination requests, named
	 * by the three `setTerminate*Flag(unsigned char)` members and by the
	 * order they appear in getV90CpBits()'s own message. All three
	 * tested with `cmpb $0x0`. */
	unsigned char terminateJa;
	unsigned char terminateCp;
	unsigned char terminateCpNot;

	/* +0x7dd1  Set to 1 by getV90CpBits() when it loads the CPnot
	 * vector, and required to be 0 for it to do so -- so the switch
	 * happens once per object. Descriptive and hedged; the object never
	 * names it. */
	unsigned char cpNotLoaded;

	/* +0x7dd2  How many bits getV90CpBits() packs into one output word
	 * (`movzbl`; the shift count is masked to five bits by the
	 * hardware). enterPhase3() sets it to 2. Descriptive and hedged. */
	unsigned char nofBitsPerSymbol;

	/*
	 * +0x7dd3 was `pad_7dd3[1]`: `nofBitsPerSymbol` ends at +0x7dd3 and
	 * `nofTransmitSequences` below is a 2-byte-aligned `unsigned short` at
	 * +0x7dd4, so natural alignment inserts exactly this one byte with the
	 * member deleted -- the existing `VPCM_OFF(nofTransmitSequences,
	 * 0x7dd4, nseq)` (VPcmFloModem.cpp) is what proves it. Zero
	 * readers/writers anywhere in the object (F10142); removed F10150.
	 */

	/* +0x7dd4, +0x7dd6  The completed-sequence counter and the minimum it
	 * must reach before CP can give way to CPnot. Both `movzwl`, compared
	 * unsigned. `setMinNofTransmitSequences` takes an `unsigned short`.
	 * enterPhase3() sets the counter to 0 and the minimum to 1, so one
	 * completed sequence is enough unless something raises it later. */
	unsigned short nofTransmitSequences;
	unsigned short minNofTransmitSequences;

	/* +0x7dd8, +0x7e2c, +0x7e80, +0x7ed4  Four parallel float arrays of
	 * VPCM_L2 entries. getUinfoValue() clears all four and fills only
	 * the third; setPhaseIIinfo() installs all four into both Phase 2
	 * records, in this order, and the third lands in the slot both
	 * printers call `L2`. The other three are offset-named: nothing
	 * establishes what they hold. */
	float array_7dd8[VPCM_L2];
	float array_7e2c[VPCM_L2];
	float L2[VPCM_L2];
	float array_7ed4[VPCM_L2];

	/* +0x7f28  A GenericIIR<float, double>, embedded, 0x34 bytes; built
	 * with (5, 5, entFiltDen, entFiltNum, 99). The last float array ends
	 * exactly here (0x7ed4 + 21*4 == 0x7f28), and the filter fills the
	 * span from there to +0x7f5c (0x7f28 + 0x34 == 0x7f5c). */
	GenericIIR<float, double> entFilt;

	/*
	 * +0x7f5c, +0x7f60, +0x7f64  A byte and two words the constructor
	 * clears last, after every member is built -- the reason `sizeof` is
	 * 0x7f68 and not 0x7f5c: the three of them plus three bytes of
	 * alignment fill the object exactly.
	 *
	 * `byte_7f5c` stays offset-named: nothing this tree has read touches
	 * it beyond the constructor's clear.
	 *
	 * `ecMode`/`ecRampCounter` are named: `runPcmModem` reads and writes
	 * both, and `VPCM_EC_RAMP_MODE`/`_START`/`_SENTINEL` below are the
	 * object's own three constants for them. `ecMode` selects whether
	 * the echo canceller's input this block is the real receive sample
	 * (any other value) or a synthetic ramp (`VPCM_EC_RAMP_MODE`,
	 * entered when the demodulator reports rate-renegotiation silence);
	 * `ecRampCounter` is the ramp's own position, reset to
	 * `VPCM_EC_RAMP_START` on every mode change and stepped by one per
	 * block while the ramp runs, capped at `VPCM_EC_RAMP_SENTINEL` --
	 * which `V92EchoCanceller::process` treats as "pass the input
	 * through untouched" (documented at the ramp's own definition,
	 * below). Usage inference: the object never prints either field's
	 * name.
	 */
	unsigned char byte_7f5c;			/* +0x7f5c         */

	/*
	 * +0x7f5d..+0x7f5f was `pad_7f5d[3]`: `byte_7f5c` ends at +0x7f5d and
	 * `ecMode` below is a 4-byte-aligned `unsigned int` at +0x7f60, so
	 * natural alignment inserts exactly these three bytes with the member
	 * deleted -- proved by adding `VPCM_OFF(ecMode, 0x7f60, ecmode)` to
	 * VPcmFloModem.cpp. Zero readers/writers anywhere in the object
	 * (F10142); removed F10150.
	 */
	unsigned int ecMode;				/* +0x7f60         */
	unsigned int ecRampCounter;			/* +0x7f64         */
};

#endif /* DSPLIB_VPCMFLOMODEM_H */

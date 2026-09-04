/*
 * V90Demodulator.h -- the V.90 receive session, and the object that owns
 * nearly everything else in the receiver.
 *
 * Reconstructed from dsplibs.o.  V90SessionFlag.h declared the class as three
 * fields and 0x1a8 bytes of `pad_`, because the only member that batch wrote
 * was `setSessionFlag`; this is the split its own comment asked the first
 * batch with real weight to make, and the two facts it recorded -- the flag at
 * +0x30, the two phase pointers at +0x1dc and +0x1e0 -- are unchanged below.
 *
 * THE SIZE IS SETTLED AT 0x298, AND NOT BY A DISPLACEMENT SCAN.  Finding F268
 * measured the scan's answer for this class as 0x28230 and showed it was a
 * scaled index into a table rather than an offset off `this`.  The oracle is
 * the allocation: `V90Modem`'s constructor reads
 *
 *     movl $0x298,(%esp); call sysdep_malloc; ... ; call V90DemodulatorC1
 *     mov  %ebx,0x4(%esi)
 *
 * -- 0x298 bytes, handed to the constructor, and stored at V90Modem+0x04,
 * which is the `demodulator` V90SessionFlag.h already had.  `enterPhase3`'s
 * largest displacement is the four-byte read at +0x294, ending at 0x298, and
 * that was not used to derive it.  Finding F291.
 *
 * THE FIELD MAP BELOW IS THE CONSTRUCTOR'S, NOT `enterPhase3`'s.  A 448-byte
 * method touches a dozen offsets; the 1002-byte constructor builds or stores
 * every subobject in the class and hands each to a callee whose mangled name
 * says what type it is.  So the pointer types here are read off manglings --
 * `V90Phase4Demodulator(V90MappingParams *, V90MappingParams *,
 * V90Demapper *, V90CP *, V90MP *, Descrambler<unsigned char, int> *,
 * V90ConnectionEvaluator *, V90Parameters *, V90Phase3Demodulator *,
 * V90AutoDigitalImpDetector *, unsigned int)` alone fixes eight of them --
 * and not off what a field is used for.
 *
 * FOUR INTERIOR BOUNDARIES FALL OUT EXACTLY, which is the check that the
 * embedded subobjects are subobjects rather than coincidence.  None of the
 * four sizes was derived here:
 *
 *     +0x06c V90PreFilter                    0x28  ends 0x094, a field
 *     +0x094 V90Resampler                          ends 0x148, a subobject
 *     +0x1e8 Descrambler<unsigned char,int>  0x20  ends 0x208, a field
 *     +0x210 V90SpectralVerifier             0x2c  ends 0x23c, a field
 *
 * Data member names are invented (finding F226) except where a diagnostic or a
 * mangling supplies one.  Offsets nothing in wave 2 explains keep `word_`,
 * `byte_` and `pad_` names rather than being guessed into meaning.
 */

#ifndef DSPLIB_V90DEMODULATOR_H
#define DSPLIB_V90DEMODULATOR_H

#include "dsplib/ResamplerTimingOffset.h"
#include "dsplib/Scrambler.h"
#include "dsplib/V90Equalizer.h"
#include "dsplib/V90Jd.h"
/*
 * `V90ConnectionEvaluator` used to be DEFINED here, because `enterPhase3`
 * reaches four words into it and nothing else in the tree touched the class.
 * It now has its own header and its own three members (task #88), so this is
 * an include and not a definition.  Nothing else moved: the field names, the
 * 0xbc size and the offset assertions are the same, and the assertions moved
 * with the class to V90ConnectionEvaluator.cpp.
 */
#include "dsplib/Agc.h"
#include "dsplib/V90ConnectionEvaluator.h"
#include "dsplib/V90ConstellationPower.h"
#include "dsplib/V90Phase2Info.h"
#include "dsplib/V90Phase3Demodulator.h"
#include "dsplib/V90PreFilter.h"
#include "dsplib/V90SpectralVerifier.h"
#include "dsplib/V92Jd.h"

/*
 * THE GUARD CLAIM THIS BLOCK DESCRIBED IS GONE, AND EVERY PARAGRAPH BELOW
 * ABOUT IT IS HISTORY.  It said this header `#define`d
 * `DSPLIB_V90PARAMETERS_H` for itself so that the named `V90Parameters` map
 * could never arrive, and that a translation unit wanting the named map had
 * to spell its accesses as indices.  Both stopped being true at task #116:
 * `V90PreFilter.h` INCLUDES `V90Parameters.h` rather than defining a second,
 * smaller `V90Parameters`, this file defines the guard nowhere, and there is
 * one definition of the class in the tree at 0x558 bytes.
 *
 * IT IS MEASURED AND NOT ARGUED.  `test/unit/t_v90p4ddec.cpp` includes this
 * header beside `V90Parameters.h` and reads `PARAMS->TRN2D_DD_LENGTH`,
 * `PARAMS->PHASE4_R_DETECTION_LENGTH` and a dozen more named fields in the
 * same translation unit, and `make phase` is green.  The block below is kept
 * because the RESAMPLER half of it is live and because the failure mode it
 * describes cost real time; read it for that and not for a restriction to
 * work around.  CLAUDE.md's own rule about a paragraph that states a live
 * defect -- check it against the tool before repeating it -- and finding
 * F6402's shape a second time.
 *
 * THE RESAMPLER AT +0x094 IS A `V90Resampler` AND HAS TO BE DECLARED AS ONE.
 *
 * It used to be modelled as its `ResamplerTimingOffset` base plus 0x68 bytes
 * of `pad_`, which was enough while nothing here had a lifecycle: no
 * constructor named it and no destructor destroyed it.  It stops being enough
 * the moment `V90Demodulator`'s pair is written, and not as a matter of taste
 * -- `ResamplerTimingOffset` has a `virtual ~ResamplerTimingOffset()` and no
 * default constructor, so with the old declaration the compiler DEMANDS a
 * mem-initializer naming one of that base's two six-argument constructors and
 * EMITS `_ZN21ResamplerTimingOffsetD1Ev` at the end of the destructor.  The
 * blob calls `_ZN12V90ResamplerC1Ejfjffj`... `_ZN12V90ResamplerD1Ev`.  Two
 * wrong symbols, neither of them suppressible.
 *
 * `V90Resampler.h` reaches for the NAMED `V90Parameters` map and this header
 * already carries the BLOCK form, which V90PreFilter.h above defines -- finding
 * F1112, the duplication that is a wart and not a design.  The guard is
 * therefore CLAIMED here so that the second definition never arrives.  That is
 * safe and it is not a new restriction: every translation unit that includes
 * this header already had the block form and already could not include
 * `V90Parameters.h`.  It is strictly less restrictive than what was here
 * before, because a TU may now include `V90Resampler.h` after this header and
 * get the class -- which `t_v90demod.cpp` records itself as unable to do.
 *
 * `V90Resampler.h` uses `V90Parameters` only as `V90Parameters *`, so the
 * block form satisfies every one of its declarations.  0x4c + 0x68 == 0xb4 ==
 * sizeof(V90Resampler), so nothing below +0x148 moves and the existing offset
 * assertions in V90Demodulator.cpp catch it if it ever does.
 *
 * WHAT IT COSTS IS THE SHAPE OF THE ERROR, and that is worth knowing before
 * you spend an hour on it.  A translation unit that wanted the NAMED map and
 * included this header used to get a REDEFINITION error naming
 * `V90Parameters`, which says exactly what happened.  It now silently gets the
 * block form instead, and the first named field it reaches fails with "no
 * member named ..." -- which points at the field rather than at the include.
 * If you see that, the answer is that this header has claimed the guard, and
 * the fix is to spell the access as an index (`params->w[0x170 / 4]`) the way
 * V90Demodulator.cpp does.
 */
#include "dsplib/V90Resampler.h"

/*
 * Named by the constructor manglings that pass them, and not modelled: this
 * class only ever holds pointers to them.
 */
class V90MappingParams;
class V90TRN2Designer;
class V90CP;
class V90MP;
class V90Demapper;
class V90ConstellationDesigner;
class V90Phase4Demodulator;

/*
 * Argument 8's type, out of the constructor's mangling
 * (`P22tagV90AdditionalCPinfo`), and the same forward declaration
 * V90Modulator.h makes for the same record on the transmit side.
 */
struct tagV90AdditionalCPinfo;

/*
 * `getAT_UD`'s argument, out of its own mangling
 * (`P21TAG_DiagnosticResults`).  Forward-declared rather than included
 * because this class only ever holds a pointer to one -- the definition is in
 * dsplib/TAG_DiagnosticResults.h and there is exactly one of it.
 */
struct TAG_DiagnosticResults;

class V90Demodulator {
public:
	/* Written -- wave 2. */
	void setSessionFlag(unsigned int flag);
	void enterPhase3();

	/*
	 * THE RETURN TYPE IS THE ONE THING THE MANGLING CANNOT GIVE, and it
	 * came from the code generation instead.  The blob's tail is
	 *
	 *     1b919  df 3c 24        fistpll (%esp)
	 *     1b920  8b 04 24        mov     (%esp),%eax
	 *
	 * -- a SIXTY-FOUR bit store with the low half taken, which is what
	 * GCC emits for a float converted to `unsigned int`; a conversion to
	 * `int` is a 32-bit `fistpl` and nothing else.  Both spellings were
	 * put through the same flags to check that (see the .cpp).  This is
	 * CLAUDE.md's forced/free rule: the store WIDTH was not the
	 * compiler's to choose.
	 */
	unsigned int getBitRate() const;

	/*
	 * NOT `void`, AND NOT DECIDABLE FURTHER.  The single exit clears
	 * `%eax` -- `add $0x34,%esp; xor %eax,%eax; pop; pop; ret` -- which a
	 * `void` member compiled through the same flags does not do, while
	 * `int` and `unsigned int` both do and are byte-identical.  So the
	 * constant zero is forced and its signedness is want of evidence; the
	 * .cpp records the experiment.  No caller in this tree.
	 */
	int sessionTermination();

	/*
	 * THE LIFECYCLE PAIR, C1/C2 at 0x1c2b0 and 0x1c6a0 (1002 B each) and
	 * D1/D2 at 0x1ad70 and 0x1b010 (669 B each).  The whole of both is in
	 * src/pump/v90/V90Demodulator.cpp; the argument names here are the
	 * mangling's types with invented spellings, since a parameter's name is
	 * no more recoverable than a data member's (finding F226).
	 *
	 * THE BLOCK THIS COMMENT USED TO CARRY IS GONE AND WAS RIGHT WHEN IT
	 * WAS WRITTEN.  It said the pair was blocked on `V90Phase4Modulator`
	 * and `ANSamToneDetector`, both then unwritten and both reached from
	 * the transmit side as well.  Both are written now, so the block
	 * expired rather than being argued away.
	 */
	V90Demodulator(unsigned int levels, V90Phase2Info *phase2,
		       V90Jd *jd, V92Jd *jdV92, tagV90DILdescriptor *dil,
		       V90MappingParams *mappingParams1,
		       V90MappingParams *mappingParams2,
		       tagV90AdditionalCPinfo *cpInfo, V90CP *cp, V90MP *mp,
		       __tHardwareCodecTypes__ codec, V90Parameters *params,
		       V90ComputationalMode compMode, unsigned int flag);
	~V90Demodulator();

	/*
	 * THE PHASE-4 AND DATA-PHASE ENTRY POINTS.  All five end in a `ret`
	 * with nothing loaded into %eax on any path -- or, where the last
	 * statement is a diagnostic, in a TAIL CALL to a `void` function --
	 * so `void` is forced here rather than being want of evidence, which
	 * is what it was while they were only declared.  Compare
	 * `sessionTermination` above, which had to keep its `int` because its
	 * single exit clears %eax and a `void` member does not.
	 */
	void enterDataSteadyState();
	void enterDataPhase();
	void enterRRN();
	void enterFPE();
	void enterPhase4();

	/*
	 * `exitPhase3` -- .text+0x1bb50, 768 bytes, and `void` for the same
	 * reason the five above are: the two exits are a bare `ret` with
	 * nothing loaded into `%eax` on either path.  It is the phase 3 to
	 * phase 4 hand-over: it reports the TRN1d RMS ratio, fills three
	 * fields of the additional-CP record, ends the DIL, enters phase 4
	 * where phase 3 terminated, designs the TRN2 constellations and
	 * resets the phase 4 demodulator.
	 */
	void exitPhase3();

	/*
	 * Fills the caller's six-element array with
	 * `V90AutoDigitalImpDetector::byte_280c`, one word per frame phase.
	 * `unsigned int *` is the mangling's (`Pj`); the count is the loop's
	 * `cmp $0x5 / jbe`, and it is `V90ADID_PHASES`.
	 */
	void getRbsPattern(unsigned int *rbs) const;

	/*
	 * The AT-command diagnostics accessor.  `const` is the mangling's
	 * (`_ZNK`), and the argument type is the only place in the object
	 * where `TAG_DiagnosticResults` is named.
	 */
	void getAT_UD(TAG_DiagnosticResults *results) const;

	void indicateRemoteRateReneg() const;

	/*
	 * Declared for the record and not defined; return types are not
	 * mangled, so `void` here is want of evidence.
	 */
	void progress(int *, unsigned int &, float *, unsigned int);
	void reset(unsigned int);
	void enterChannelVerification(short, short);
	void reInit();

	/* --- data members; see the file comment on the naming --- */

	/*
	 * +0x000  WAS `word_00`, "nothing in wave 2 reads it", and the
	 * constructor is what named it: argument 11 arrives at `0x8c(%esp)`,
	 * is handed to `V90PreFilter`'s constructor as its
	 * `__tHardwareCodecTypes__` first argument, and is then stored here
	 * with `mov %edx,(%ebx)`.  So the OFFSET is the constructor's and the
	 * TYPE is V90PreFilter's mangling, which is the only kind of name this
	 * file trusts.  Nothing reads it back.
	 */
	__tHardwareCodecTypes__ codecType;

	/*
	 * +0x004  The constructor's second argument.  `enterPhase3` calls
	 * `printInfo()` on it and then reads three of its fields for
	 * `V90Phase3Demodulator::reset`.
	 */
	V90Phase2Info *phase2Info;

	/* +0x008, +0x00c, +0x010  Constructor arguments 3, 4 and 5, passed
	 * straight through to `V90Phase3Demodulator::reset`. */
	V90Jd *jd;
	V92Jd *jdV92;
	tagV90DILdescriptor *dil;

	/* +0x014, +0x018  Where V90Phase4Demodulator's first two arguments
	 * come from. */
	V90MappingParams *mappingParams;
	V90MappingParams *mappingParamsAlt;

	/* +0x01c  `sysdep_malloc(8)` and V90TRN2Designer(V90Parameters *,
	 * V90ConstellationPower *). */
	V90TRN2Designer *trn2Designer;

	/*
	 * +0x020  WAS `pad_20`.  Argument 8 arrives at `0x80(%esp)` and is
	 * stored here with `mov %edx,0x20(%ebx)`; the type is the constructor
	 * mangling's `P22tagV90AdditionalCPinfo`, and the name is the one
	 * V90Modulator.h already gives the same record at ITS +0x18.  Nothing
	 * reconstructed reads it back.
	 */
	tagV90AdditionalCPinfo *additionalCPinfo;

	/* +0x024, +0x028  V90Phase4Demodulator's fourth and fifth arguments. */
	V90CP *cp;
	V90MP *mp;

	/*
	 * +0x02c  The constructor's twelfth argument.  Six callees take it as
	 * a `V90Parameters *`, and `enterPhase3` reads +0x84, +0x264 and
	 * +0x278 out of it and dereferences the pointer at its +0x00.
	 */
	V90Parameters *params;

	/*
	 * +0x030  What `setSessionFlag` stores, and what the constructor
	 * hands to `V90Phase3Demodulator` as its own `sessionFlag`.  Non-zero
	 * is the V.92 session, which is the condition `enterPhase3` ends on.
	 */
	unsigned int sessionFlag;

	/*
	 * +0x034  A STATE, NOT A LATCH, and the name is older than the
	 * evidence.  `enterPhase3` returns immediately when this is exactly 1
	 * and sets it to 1 otherwise, which reads as a latch until three other
	 * members are in the tree: `enterChannelVerification` sets it to 5,
	 * and `sessionTermination` tests it against 3 for an argument it
	 * prints as `isDataState`.  So 1 is phase 3, 3 is the data state, 5 is
	 * channel verification, and testing for 1 rather than for non-zero is
	 * the blob's and matters -- any other value does NOT suppress
	 * `enterPhase3`'s work.  The name is invented either way (finding F226)
	 * and is left as it is rather than renamed under eight parallel
	 * worktrees; finding F1273.
	 */
	unsigned int inPhase3;

	/*
	 * +0x038  Saved into +0x044 and cleared, in that order.  Nothing in
	 * wave 2 says what either holds.
	 */
	unsigned int word_38;

	/*
	 * +0x03c  Cleared by `enterPhase3`, and set to 0x20 by its one
	 * failure exit -- the V.92 Lite retrain.
	 */
	unsigned int word_3c;

	unsigned int word_40;		/* +0x040 cleared by `enterPhase3`   */
	unsigned int word_44;		/* +0x044 receives the old +0x038    */

	/*
	 * +0x048  WAS `pad_48`, and the three phase-entry members are what
	 * modelled it.  Each stores a deadline built from
	 * `phase2Info->rtd` with a single `lea`, and nothing in this batch
	 * reads it back:
	 *
	 *     enterRRN, enterFPE   lea 0x10680(%edx,%edx,1)   0x10680 + 2*rtd
	 *     enterPhase4          lea 0x28230(%edx,%edx,4)   0x28230 + 5*rtd
	 *
	 * A word, and no more than that is claimed.  At the 8000 Hz
	 * downstream rate the two constants are 8.4 s and 20.6 s, which is
	 * the right order for a phase timeout in samples and is why the name
	 * is not `word_48` -- but the comparison site is in `progress`, which
	 * is unwritten, so "deadline" is inference and "sample count" is
	 * inference on top of it.  MODELLED, UNNAMED, per CLAUDE.md.
	 */
	unsigned int word_48;		/* +0x048                            */

	/*
	 * +0x04c  EMBEDDED, and now modelled: `V90Demodulator::reset` does
	 * `lea 0x4c(%esi),%ebx`, calls `Agc<float>::reset` on it, and then
	 * writes two of its fields through the SAME register -- `agc+0x18`
	 * and `agc+0x0c`, which `Agc.h` already names `blockLen` and `ref`.
	 * That the caller reconfigures the AGC immediately after resetting it
	 * is what identifies the sub-object; 32 bytes takes it to +0x6c,
	 * where the prefilter starts.
	 */
	Agc<float> agc;			/* +0x04c */

	/*
	 * +0x06c  EMBEDDED: `lea 0x6c(%ebx),%ebp` in the constructor before
	 * `V90PreFilter(__tHardwareCodecTypes__, V90Phase2Info *,
	 * V90Parameters *)`, and `lea 0x6c(%esi),%ebx` in `enterPhase3`,
	 * which calls four of its members on that address.
	 */
	V90PreFilter preFilter;

	/*
	 * +0x094  EMBEDDED, and it is a V90Resampler -- the constructor builds
	 * one here with `V90Resampler(unsigned, float, unsigned, float,
	 * V90Parameters *, float, unsigned)` and the destructor ends by calling
	 * `_ZN12V90ResamplerD1Ev` on it.  It was modelled as its
	 * `ResamplerTimingOffset` base plus 0x68 bytes of `pad_e0` until the
	 * lifecycle pair was written; see the guard claim at the top of this
	 * file for why that stopped working and what it costs to fix.  Finding
	 * F228 is why a base at offset 0 is what a call on the derived object's
	 * address looks like, which is still how `enterPhase3` reaches
	 * `setTimingOffset` here.
	 */
	V90Resampler resampler;

	/*
	 * +0x148  EMBEDDED, 0x90 bytes, ending exactly at the equaliser
	 * pointer.  WAS `pad_148`, "a V90ConstellationPower, not modelled":
	 * the constructor builds one here with `V90ConstellationPower()` and
	 * hands this address to `V90TRN2Designer` and
	 * `V90ConstellationDesigner` as their last argument, which is where
	 * the type came from and why the pad said so.  The class itself is
	 * still 0x90 unmodelled bytes (V90ConstellationPower.h); what changed
	 * is that the compiler now constructs and destroys it, because a
	 * `pad_` cannot.
	 */
	V90ConstellationPower constellationPower;

	/*
	 * +0x1d8  `sysdep_malloc(0x150)`, and V90Equalizer's largest modelled
	 * field ends at 0x148 -- consistent, and not used to derive anything.
	 */
	V90Equalizer *equalizer;

	/* +0x1dc  `sysdep_malloc(0x42c)` == sizeof(V90Phase3Demodulator). */
	V90Phase3Demodulator *phase3Demodulator;

	/* +0x1e0  `sysdep_malloc(0x351c)`; the class is not modelled here. */
	V90Phase4Demodulator *phase4Demodulator;

	/* +0x1e4  `sysdep_malloc(0x1eb8)`. */
	V90Demapper *demapper;

	/*
	 * +0x1e8  EMBEDDED, 0x20 bytes, built with (0x12, 0x17, 0x63) and
	 * handed to V90Phase4Demodulator as its sixth argument -- which is
	 * where its type comes from.  A DIFFERENT INSTANTIATION from the
	 * `<int,int>` inside V90Phase3Demodulator.
	 */
	Descrambler<unsigned char, int> descrambler;

	/* +0x208  `sysdep_malloc(0x54)`. */
	V90ConstellationDesigner *constellationDesigner;

	/*
	 * +0x20c  `sysdep_malloc(0xbc)`.  `enterPhase3` clears four of its
	 * fields -- +0x70, +0x74, +0x84 and +0x88 -- and does nothing else
	 * with it.
	 */
	V90ConnectionEvaluator *connectionEvaluator;

	/*
	 * +0x210  EMBEDDED, 0x2c bytes, ending exactly at the field below.
	 * `enterPhase3` calls `reset()` on this address; the constructor
	 * builds it with `V90SpectralVerifier(V90Parameters *)` and passes the
	 * same address to V90Phase3Demodulator's constructor.
	 */
	V90SpectralVerifier spectralVerifier;

	/* +0x23c  Built by the constructor from `params` and shared with the
	 * phase 3 and phase 4 demodulators. */
	V90AutoDigitalImpDetector *autoDigitalImpDetector;

	/*
	 * +0x240  WAS `pad_240[4]`, "nothing reconstructed reads it", and
	 * `exitPhase3` both READS it and NAMES it.  `flds 0x240(%edi)` at
	 * 0x1bb81 is the only access anywhere in the object, and the value
	 * goes straight into the `%c%d.%08d` triple of
	 *
	 *     "V90Demodulator: TRN1d RMS Ratio = %c%d.%08d\r\n"
	 *
	 * so the WIDTH and the TYPE are the load's and the NAME is the
	 * author's own -- CLAUDE.md's evidence rule 1, the strongest kind
	 * there is.  Nothing in the object WRITES it, which is consistent
	 * with `progress` being unwritten and is not evidence either way.
	 */
	float trn1dRmsRatio;		/* +0x240                            */

	/*
	 * +0x244 .. +0x25c  FIVE HEAP BLOCKS, ALL SIZED FROM THE
	 * CONSTRUCTOR'S FIRST ARGUMENT, and all five freed -- with a bare
	 * `sysdep_free` and no destructor -- by `~V90Demodulator`.  They were
	 * `pad_240`, `pad_250` and `pad_25c` until the constructor was read;
	 * the five allocations are consecutive and the widths come off the
	 * address arithmetic in front of each one:
	 *
	 *     1c7da:  8d 3c b5 00 00 00 00  lea  0x0(,%esi,4),%edi
	 *     1c7e9:  e8 ..                 call sysdep_malloc  -> +0x244
	 *     1c7f4:  8d 04 36              lea  (%esi,%esi,1),%eax   ; n * 2
	 *     1c7fb:  01 f0                 add  %esi,%eax            ; n * 3
	 *     1c7fd:  c1 e0 02              shl  $0x2,%eax            ; n * 12
	 *     1c803:  e8 ..                 call sysdep_malloc  -> +0x248
	 *     1c80e:  c1 e6 03              shl  $0x3,%esi            ; n * 8
	 *     1c811:  mov %edi,(%esp)                                 ; n * 4
	 *     1c814:  e8 ..                 call sysdep_malloc  -> +0x250
	 *     1c81f:  mov %esi,(%esp)                                 ; n * 8
	 *     1c822:  e8 ..                 call sysdep_malloc  -> +0x254
	 *     1c82d:  mov %esi,(%esp)                                 ; n * 8
	 *     1c832:  e8 ..                 call sysdep_malloc  -> +0x25c
	 *
	 * `%esi` is the first argument throughout and `%edi` holds `n * 4`
	 * across the middle three.  So the widths are 4, 12, 4, 8 and 8 bytes
	 * an element -- and the ELEMENT TYPES are not derivable from that, so
	 * all five are `void *` until something that reads them is written.
	 * The `n * 12` one is the only interesting shape: three words an
	 * element, built as `(n + n) + n` and then shifted, which is a
	 * multiply the compiler chose and not a `* 3` in the source that can
	 * be read back.
	 */
	void *array_244;		/* +0x244 n * 4 bytes                */
	void *array_248;		/* +0x248 n * 12 bytes               */
	unsigned int word_24c;		/* +0x24c zeroed by the constructor
					 *        AND by reset               */
	void *array_250;		/* +0x250 n * 4 bytes                */
	void *array_254;		/* +0x254 n * 8 bytes                */
	unsigned int word_258;		/* +0x258 zeroed by the constructor
					 *        AND by reset               */
	void *array_25c;		/* +0x25c n * 8 bytes                */
	unsigned int word_260;		/* +0x260 zeroed by reset            */

	unsigned int word_264;		/* +0x264 zeroed by the constructor  */
	unsigned int word_268;		/* +0x268 zeroed by the constructor  */
	unsigned int word_26c;		/* +0x26c zeroed by the constructor  */
	unsigned int word_270;		/* +0x270 zeroed by reset            */

	/*
	 * +0x274  WAS `pad_274[4]`, "nothing reads it", and `progress` both
	 * writes it and reads it back.  It takes
	 * `equalizer->meanErrorEnergyMean` at 0x1e5dd -- on the arm where Ed
	 * arrives while a silence RRN is outstanding -- and it is the
	 * constellation designer's THIRD argument at 0x1d82f, which that
	 * member's mangling spells `f`.  So the width is the store's, the type
	 * is the callee's (CLAUDE.md rule 2), and those two sites are the only
	 * accesses anywhere in the object.
	 *
	 * IT KEEPS THE OFFSET NAME.  What the pair of sites bounds is the
	 * ROLE -- "the mean error as it stood when the redesign was armed" --
	 * and not a meaning the object states: no diagnostic prints it, no
	 * other member touches it, and the designer's own parameter is a
	 * general pdSNR slot that the other call site fills from a different
	 * expression entirely.  3120's rule.
	 */
	float float_274;		/* +0x274                            */
	unsigned int word_278;		/* +0x278 zeroed by the constructor  */
	unsigned int word_27c;		/* +0x27c zeroed by the constructor  */

	/* +0x280  Zeroed by the constructor, by `enterPhase3` and by `reset`. */
	unsigned char byte_280;

	unsigned char pad_281[3];	/* +0x281 alignment                  */

	/*
	 * +0x284/+0x288 and +0x28c/+0x290  TWO PRINT-PERIOD COUNTERS, named on
	 * the strength of BOTH the parameter names `progress` copies into
	 * +0x288/+0x290 and the diagnostics beside the counters at +0x284/
	 * +0x28c -- CLAUDE.md's rule 1 (a format string) and rule 2 (a typed
	 * source), agreeing.
	 *
	 * +0x288 and +0x290 ARE NOT ONE PARAMETER EACH; they are reloaded from
	 * a DIFFERENT `V90Parameters` slot depending which phase-entry member
	 * last ran -- `enterPhase3` copies +0x264/+0x278
	 * (`ERROR_ENERGY_PRINT_PERIOD_PHASE3`/`TIMING_OFFSET_PRINT_PERIOD_
	 * PHASE3`), `enterRRN`/`enterFPE`/`enterPhase4` copy +0x268/+0x27c
	 * (the `_PHASE4` pair) and the data-phase entry copies +0x26c/+0x280
	 * (the `_DATA` pair) -- but the ROLE is the same constant across every
	 * site: "how many samples to wait before the next print", which is
	 * what makes this a single pair of names and not three.  `progress`'s
	 * common tail (see the .cpp) then runs `if (counter + nofIn < period)
	 * counter += nofIn; else { counter = 0; <print>; }` for each pair
	 * unconditionally, whichever phase is active.
	 *
	 * +0x284 and +0x28c ARE THE COUNTERS the same tail advances and resets,
	 * and the diagnostic each one gates names the QUANTITY rather than the
	 * counter itself -- "V90Demodulator: Error Energy = %c%d.%03d" off
	 * `equalizer->meanErrorEnergyCurrent` when +0x284 rolls over, and
	 * "V90Demodulator: Timing Offset [ppm]  = %c%d.%03d" off
	 * `resampler.getTimingOffsetPPM()` when +0x28c does -- so the counter
	 * names are usage inference (the companion of a named period, the same
	 * shape `avePdsnrNofSymbols` is named on in `V90ConnectionEvaluator.h`)
	 * and the period names are rule 1/2 together.  Finding F10134.
	 */
	unsigned int errorEnergyPrintCounter;		/* +0x284 */
	unsigned int errorEnergyPrintPeriod;		/* +0x288 */
	unsigned int timingOffsetPrintCounter;		/* +0x28c */
	unsigned int timingOffsetPrintPeriod;		/* +0x290 */

	/*
	 * +0x294  WAS `word_294`, and TWO INDEPENDENT DERIVATIONS name it.
	 * `V90Demodulator::reset(unsigned int quickConnect)` stores its
	 * argument here and into `equalizer->quickConnect` in the same
	 * breath -- a field this tree already names -- and `exitPhase3`
	 * hands it to `V90Phase4Demodulator::reset` as that member's fourth
	 * argument, which lands in `V90Phase4Demodulator::quickConnect`,
	 * named in finding F7472 from the format string that prints it.
	 * Neither reading knew about the other, which is what took this off
	 * usage inference and onto CLAUDE.md's rule 2.
	 *
	 * It is also copied into `phase3Demodulator->quickConnect` AFTER
	 * `V90Phase3Demodulator::reset` has zeroed that field, which is the
	 * one place the two members of wave 2 interact observably, and
	 * `enterDataPhase` picks the shorter linear-mapping study when it is
	 * set.
	 */
	unsigned int quickConnect;
};

#endif /* DSPLIB_V90DEMODULATOR_H */

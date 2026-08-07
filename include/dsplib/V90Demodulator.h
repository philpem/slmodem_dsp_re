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
 * THE SIZE IS SETTLED AT 0x298, AND NOT BY A DISPLACEMENT SCAN.  Finding 268
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
 * that was not used to derive it.  Finding 291.
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
 * Data member names are invented (finding 226) except where a diagnostic or a
 * mangling supplies one.  Offsets nothing in wave 2 explains keep `word_`,
 * `byte_` and `pad_` names rather than being guessed into meaning.
 */

#ifndef DSPLIB_V90DEMODULATOR_H
#define DSPLIB_V90DEMODULATOR_H

#include "dsplib/ResamplerTimingOffset.h"
#include "dsplib/Scrambler.h"
#include "dsplib/V90Equalizer.h"
#include "dsplib/V90Jd.h"
#include "dsplib/V90Phase2Info.h"
#include "dsplib/V90Phase3Demodulator.h"
#include "dsplib/V90PreFilter.h"
#include "dsplib/V90SpectralVerifier.h"
#include "dsplib/V92Jd.h"

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
 * Modelled only as far as `V90Demodulator::enterPhase3` reaches into it, which
 * is four words it clears.  The SIZE is not a guess: the constructor allocates
 * it with `sysdep_malloc(0xbc)` (finding 291).  Its own members are not in
 * task #60 and nothing here declares them.
 */
class V90ConnectionEvaluator {
public:
	unsigned char pad_00[0x70];	/* +0x00 not modelled                */
	unsigned int word_70;		/* +0x70 cleared by enterPhase3      */
	unsigned int word_74;		/* +0x74 cleared by enterPhase3      */
	/*
	 * +0x78 and +0x7c were `pad_78` until the VPcmFloModem batch read a
	 * second function that touches this object: `getV90CpBits` copies
	 * +0x78 to +0x7c each time a CP sequence finishes.  Two batches, two
	 * functions, one object -- neither would have found both.
	 */
	unsigned int word_78;		/* +0x78 copied to word_7c           */
	unsigned int word_7c;		/* +0x7c                             */
	unsigned char pad_80[4];	/* +0x80 not modelled                */
	unsigned int word_84;		/* +0x84 cleared by enterPhase3      */
	unsigned int word_88;		/* +0x88 cleared by enterPhase3      */
	unsigned char pad_8c[0x30];	/* +0x8c not modelled, to 0xbc       */
};

class V90Demodulator {
public:
	/* Written -- wave 2. */
	void setSessionFlag(unsigned int flag);
	void enterPhase3();

	/*
	 * Declared for the record and not defined; return types are not
	 * mangled, so `void` here is want of evidence.  The constructor and
	 * destructor are not declared at all, for the reason
	 * V90Phase3Modulator.h gives.
	 *
	 *     V90Demodulator(unsigned int, V90Phase2Info *, V90Jd *, V92Jd *,
	 *                    tagV90DILdescriptor *, V90MappingParams *,
	 *                    V90MappingParams *, tagV90AdditionalCPinfo *,
	 *                    V90CP *, V90MP *, __tHardwareCodecTypes__,
	 *                    V90Parameters *, V90ComputationalMode,
	 *                    unsigned int)              C1,C2   1002 B
	 *     ~V90Demodulator()                         D1,D2    669 B
	 */
	void progress(int *, unsigned int &, float *, unsigned int);
	void exitPhase3();
	void sessionTermination();
	void enterDataSteadyState();
	void reset(unsigned int);
	void enterDataPhase();
	void enterRRN();
	void enterChannelVerification(short, short);
	void enterFPE();
	void enterPhase4();
	void getBitRate() const;
	void getRbsPattern(unsigned int *) const;
	void reInit();
	void indicateRemoteRateReneg() const;

	/* --- data members; see the file comment on the naming --- */

	unsigned int word_00;		/* +0x000 nothing in wave 2 reads it */

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

	unsigned char pad_20[4];	/* +0x020 nothing in wave 2 reads it */

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
	 * +0x034  `enterPhase3` returns immediately when this is exactly 1 and
	 * sets it to 1 otherwise, so it is the "phase 3 has been entered"
	 * latch.  Testing for 1 rather than for non-zero is the blob's, and it
	 * matters: any other non-zero value does NOT suppress the work.
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

	unsigned char pad_48[0x24];	/* +0x048 incl. an Agc<float> at
					 *        +0x04c, not modelled       */

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
	 * V90Parameters *, float, unsigned)`.  Only its ResamplerTimingOffset
	 * base subobject is modelled, because `enterPhase3` calls
	 * `setTimingOffset` on this address and nothing in wave 2 reaches any
	 * other part of it.  Finding 228 is why a base at offset 0 is what a
	 * call on the derived object's address looks like.
	 */
	ResamplerTimingOffset resampler;

	unsigned char pad_e0[0x68];	/* +0x0e0 the rest of V90Resampler,
					 *        which ends at +0x148       */

	unsigned char pad_148[0x90];	/* +0x148 a V90ConstellationPower,
					 *        not modelled               */

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

	unsigned char pad_240[0x24];	/* +0x240 nothing in wave 2 reads it */

	unsigned int word_264;		/* +0x264 zeroed by the constructor  */
	unsigned int word_268;		/* +0x268 zeroed by the constructor  */
	unsigned int word_26c;		/* +0x26c zeroed by the constructor  */
	unsigned char pad_270[8];	/* +0x270 nothing in wave 2 reads it */
	unsigned int word_278;		/* +0x278 zeroed by the constructor  */
	unsigned int word_27c;		/* +0x27c zeroed by the constructor  */

	/* +0x280  Zeroed by the constructor and again by `enterPhase3`. */
	unsigned char byte_280;

	unsigned char pad_281[7];	/* +0x281 nothing in wave 2 reads it */

	/* +0x288  `enterPhase3` copies `params`+0x264 into it. */
	unsigned int word_288;

	unsigned char pad_28c[4];	/* +0x28c nothing in wave 2 reads it */

	/* +0x290  `enterPhase3` copies `params`+0x278 into it. */
	unsigned int word_290;

	/*
	 * +0x294  Copied into `phase3Demodulator->word_410` AFTER
	 * `V90Phase3Demodulator::reset` has zeroed that field, which is the
	 * one place the two members of wave 2 interact observably.
	 */
	unsigned int word_294;
};

#endif /* DSPLIB_V90DEMODULATOR_H */

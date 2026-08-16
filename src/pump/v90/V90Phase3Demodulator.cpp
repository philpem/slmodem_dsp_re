/*
 * V90Phase3Demodulator::reset -- put the receiver's phase 3 back to a known
 * state, from dsplibs.o.
 *
 * ELEVEN ARGUMENTS AND FIVE OF THEM ARE NEVER READ AGAIN.  The method stores
 * eight of its arguments into the object, resets five subobjects and then
 * picks one of four openings according to the state it was handed.  What can
 * go wrong is which offset each argument lands at, which of the five
 * subobjects are reset in which order, and which opening a state selects --
 * so test/unit/t_v90p3dreset.cpp compares the whole 0x42c object and every
 * subobject it touches, rather than the fields this file happens to name.
 *
 * THE FOUR OPENINGS DIFFER BY VERY LITTLE, WHICH IS THE TRAP.  Naming them by
 * the diagnostics the blob emits for them:
 *
 *     state      diagnostic          resetLinearMapping   modulator's 4th arg
 *     0          WaitForSd           yes                  0
 *     3          TRN1dKnownData      yes                  reset's 4th arg
 *     26         WaitForQTS          NO                   0
 *     anything   IRREGULAR (%d)      yes                  0
 *
 * so WaitForSd and IRREGULAR differ ONLY in the text they print, and are
 * distinguishable only with `dsplibs_debug_level > 1` and a transcript
 * comparison.  A test that runs at level 0 cannot tell them apart, and a
 * mutation that folds one into the other survives it.  Finding 292.
 *
 * The modulator's STATE argument is 2 -- P3M_STATE_TRN1D -- from all four,
 * and its Jd, V92Jd and DIL pointers are null from all four, even though the
 * demodulator was handed non-null ones and has just stored them.  That is the
 * object's, not an omission.
 */

#include <stddef.h>

extern "C" {
#include "dsplib/encode.h"
#include "dsplib/modem_params.h"
#include "dsplib/pcm.h"
#include "dsplib/debug.h"
}

#include "dsplib/V90Dil.h"
#include "dsplib/V90Phase3Demodulator.h"
#include "dsplib/V92Jd.h"

/*
 * The layout, asserted where the reconstruction depends on it.  Guarded on a
 * 32-bit pointer because `check64` compiles the same source for a host whose
 * pointers are eight bytes, where none of these offsets can hold.
 */
#if defined(__SIZEOF_POINTER__) && __SIZEOF_POINTER__ == 4

#define P3D_OFF(field, off, tag) \
	typedef char v90p3d_off_##tag[ \
	    ((int)__builtin_offsetof(V90Phase3Demodulator, field) == (off)) \
	    ? 1 : -1]

P3D_OFF(autoDigitalImpDetector,	0x000, adid);
P3D_OFF(word_04,		0x004, word04);
P3D_OFF(sessionFlag,		0x008, sessionflag);
P3D_OFF(params,			0x00c, params);
P3D_OFF(pcmType,		0x010, pcmtype);
P3D_OFF(word_14,		0x014, word14);
P3D_OFF(ucode,			0x018, ucode);
P3D_OFF(ucodeLevel,		0x01a, ucodelevel);
P3D_OFF(dil,			0x01c, dil);
P3D_OFF(jd,			0x020, jd);
P3D_OFF(jdV92,			0x024, jdv92);
P3D_OFF(state,			0x028, state);
P3D_OFF(word_2c,		0x02c, word2c);
P3D_OFF(word_30,		0x030, word30);
P3D_OFF(phase3Modulator,	0x034, phase3modulator);
P3D_OFF(word_3cc,		0x3cc, word3cc);
P3D_OFF(descrambler,		0x3d0, descrambler);
P3D_OFF(sdDetector,		0x3f0, sddetector);
P3D_OFF(word_3f4,		0x3f4, word3f4);
P3D_OFF(byte_3f9,		0x3f9, byte3f9);
P3D_OFF(word_3fc,		0x3fc, word3fc);
P3D_OFF(short_400,		0x400, short400);
P3D_OFF(word_404,		0x404, word404);
P3D_OFF(word_408,		0x408, word408);
P3D_OFF(dilLength,		0x40c, dillength);
P3D_OFF(word_410,		0x410, word410);
P3D_OFF(short_414,		0x414, short414);
P3D_OFF(float_418,		0x418, float418);
P3D_OFF(verificationStatus,	0x41c, verifstatus);
P3D_OFF(word_420,		0x420, word420);
P3D_OFF(byte_424,		0x424, byte424);
P3D_OFF(ansamToneDetector,	0x428, ansam);

/*
 * THE SIZE IS ASSERTED, unlike the three classes still in V90SessionFlag.h,
 * because `V90Demodulator`'s constructor allocates exactly this many bytes
 * before calling this class's constructor.  Finding 291.
 */
typedef char v90p3d_size[(sizeof(V90Phase3Demodulator) == 0x42c) ? 1 : -1];

#endif

void
V90Phase3Demodulator::reset(PcmType pcmTypeArg, unsigned char ucodeArg,
			    Phase3DemodulatorState stateArg,
			    unsigned int word2cArg, V90Jd *jdArg,
			    V92Jd *jdV92Arg, tagV90DILdescriptor *dilArg,
			    short altRbs, short short414Arg,
			    float float418Arg, unsigned int word14Arg)
{
	unsigned int modulatorWord;

	/*
	 * The flag is printed BEFORE anything is stored, and it is the
	 * object's own field rather than an argument -- this method never
	 * receives one.
	 */
	if (DSPLIB_DEBUG_ON())
		dsplibs_debug_printf("V90Phase3Demodulator: Reset called, "
				     "sessionFlag = %d !\r\n", sessionFlag);

	byte_424 = 0;
	short_414 = short414Arg;
	word_30 = 0;
	pcmType = pcmTypeArg;
	word_14 = word14Arg;
	float_418 = float418Arg;
	word_2c = word2cArg;
	state = stateArg;

	/*
	 * MU-LAW IS `== 0`, NOT `!= 1`.  The blob tests the argument with
	 * `test %esi,%esi`, so every non-zero value takes the A-law arm --
	 * which matters because PcmType has two enumerators and the field is
	 * a whole word.
	 *
	 * The complement-then-decode is the same one V90Phase3Modulator uses
	 * for its segment and DIL codes, with the same two constants.
	 */
	if (pcmTypeArg == PCM_TYPE_MU_LAW)
		ucodeLevel = (short)ulaw2linear(
		    (unsigned char)((ucodeArg & 0x7f) ^ 0xff));
	else
		ucodeLevel = (short)alaw2linear(
		    (unsigned char)((ucodeArg & 0x7f) ^ 0xd5));

	word_3cc.prev_ = 0;
	ucode = ucodeArg;
	dil = dilArg;

	/* Always 0, never a value out of an argument. */
	descrambler.reset(0);

	jd = jdArg;
	if (jdArg != NULL)
		jdArg->unPackReset();

	jdV92 = jdV92Arg;
	if (jdV92Arg != NULL) {
		jdV92Arg->unPackJdReset();
		/*
		 * The second call re-loads the pointer out of the object,
		 * which is only observable if the first one changed it.  It is
		 * spelled that way here because the blob spells it that way.
		 */
		jdV92->unPackJdPhaseReset();
	}

	word_404 = 0;
	sdDetector->reset();

	/*
	 * The detector is handed the object's `pcmType`, not the argument --
	 * the same value, but read back from +0x10.
	 */
	autoDigitalImpDetector->reset(ucodeArg, pcmType, altRbs);

	byte_3f9 = 0;
	word_04 = 0;
	word_3fc = 0;
	short_400 = 0;

	switch ((int)stateArg) {
	case P3D_STATE_WAIT_FOR_SD:
		if (DSPLIB_DEBUG_ON())
			dsplibs_debug_printf("V90Phase3Demodulator: initial "
					     "state set to WaitForSd\r\n");
		autoDigitalImpDetector->resetLinearMapping();
		modulatorWord = 0;
		break;

	case P3D_STATE_TRN1D_KNOWN_DATA:
		if (DSPLIB_DEBUG_ON())
			dsplibs_debug_printf("V90Phase3Demodulator: initial "
					     "state set to TRN1dKnownData\r\n");
		autoDigitalImpDetector->resetLinearMapping();
		modulatorWord = word2cArg;
		break;

	case P3D_STATE_WAIT_FOR_QTS:
		/*
		 * The ONE opening that does not reset the linear mapping.  The
		 * gate jumps past it whether or not the message is printed.
		 */
		if (DSPLIB_DEBUG_ON())
			dsplibs_debug_printf("V90Phase3Demodulator: initial "
					     "state set to WaitForQTS\r\n");
		modulatorWord = 0;
		break;

	default:
		if (DSPLIB_DEBUG_ON())
			dsplibs_debug_printf("V90Phase3Demodulator: initial "
					     "state set to IRREGULAR !!!!! "
					     "(%d)\r\n", (int)stateArg);
		autoDigitalImpDetector->resetLinearMapping();
		modulatorWord = 0;
		break;
	}

	/*
	 * The modulator gets the ARGUMENT's pcmType and ucode, not the fields
	 * just written from them, and null Jd/V92Jd/DIL pointers whatever this
	 * method was handed.
	 */
	phase3Modulator.reset(pcmTypeArg, ucodeArg, P3M_STATE_TRN1D,
			      modulatorWord, NULL, NULL, NULL, 0);

	word_408 = 0;
	dilLength = calculateDilLength(dil, pcmType);
	word_410 = 0;
}

/*
 * clearVerificationStatus -- one gated diagnostic and one store.
 *
 * `V90Demodulator::reInit` and `V90Demodulator::enterChannelVerification` are
 * the two callers, and both reach it through the demodulator's +0x1dc.  The
 * message is the object's own words for what the member is.
 *
 * The store is DUPLICATED in the object -- the arm at 0x20d9c and the one at
 * 0x20d82 are the same three instructions -- which is the tail of an `if` the
 * compiler chose to copy rather than to join, not two writes.
 */
void
V90Phase3Demodulator::clearVerificationStatus()
{
	if (DSPLIB_DEBUG_ON())
		dsplibs_debug_printf(
		    "V90Phase3Demodulator: clearVerificationStatus called\r\n");

	verificationStatus = 0;
}

/* ==================================================== the lifecycle pair */

#include "dsplib/sysdep.h"
/*
 * The BLOCK form of `V90Parameters`, because the constructor reads four
 * unnamed slots out of it and this file's `reset` already compiles against a
 * forward declaration.  No translation unit may hold both definitions;
 * finding 1112.
 */
#include "dsplib/V90PreFilter.h"
#include "dsplib/ANSamToneDetector.h"

/*
 * THE TWO CONSTRUCTORS ARE CALLED BY THEIR MANGLED NAMES, and that is this
 * tree's settled idiom rather than a workaround invented here:
 * `src/pump/v90/V90BitsToSymbol.cpp` sets out the argument in full and
 * `V90Modulator.cpp` and `V92Precoder.cpp` do the same.  The build is
 * `-nostdinc++`, so there is no <new>; declaring a replacement global
 * `operator new` inline is ill-formed; and a user-declared PLACEMENT form
 * makes GCC emit a null test the blob does not have.  The original almost
 * certainly wrote `new V90SdDetector(...)` over an inline `operator new`, and
 * the instruction sequence is the same either way -- only the spelling
 * differs.  The destructor needs no trick at all, because an explicit
 * destructor call is ordinary C++.
 */
extern "C" {
void v90p3d_sd_ctor(void *self, float a, float b, float c, unsigned int d)
	asm("_ZN13V90SdDetectorC1Efffj");
void v90p3d_ansam_ctor(void *self, unsigned int s1, unsigned int s2, float t,
		       unsigned int flag, float ratio, unsigned int rate,
		       unsigned int blockLen, unsigned int blockSize)
	asm("_ZN17ANSamToneDetectorC1Ejjfjfjjj");
}

/* The four parameter-block slots the SD detector is built from. */
#define PARAMS_SD_THRESH_08	(0x284 / 4)	/* float */
#define PARAMS_SD_THRESH_0C	(0x288 / 4)	/* float */
#define PARAMS_SD_VALUE_10	(0x28c / 4)	/* float */
#define PARAMS_SD_LIMIT		(0x290 / 4)	/* int   */

/*
 * The descrambler's three constants, and the ANSam detector's eight.  All
 * eleven are immediates in the object, so they are constants of the source
 * and not values derived from anything.  0x48960000 is 307200.0f and
 * 0x3f000000 is 0.5f; 0x1f40 is 8000, which `ANSamToneDetector.h` records as
 * the sample rate that selects the 13-tap coefficient pair.
 */
#define P3D_DSC_TAP1		0x12u
#define P3D_DSC_TAIL		0x17u
#define P3D_DSC_OUT		0x63u

#define P3D_ANSAM_SAMPLES1	0x190u
#define P3D_ANSAM_SAMPLES2	0x64u
#define P3D_ANSAM_THRESHOLD	307200.0f
#define P3D_ANSAM_RATIO		0.5f
#define P3D_ANSAM_SAMPLE_RATE	8000u
#define P3D_ANSAM_BLOCK_LEN	0x32u
#define P3D_ANSAM_BLOCK_SIZE	0x63u

/*
 * `V90Phase3Demodulator::V90Phase3Demodulator` -- 365 bytes at 0x212c0 (C1)
 * and again at 0x21430 (C2).
 *
 * `this` is the first STACK argument, so after `push edi; push esi; push ebx;
 * sub $0x30` the frame is this 0x40, params 0x44, verifier 0x48, sessionFlag
 * 0x4c, adid 0x50.
 *
 * ARGUMENT 2 IS ACCEPTED AND DROPPED.  `0x48(%esp)` appears nowhere in the
 * 365 bytes: the entry loads are 0x40, 0x44 and 0x4c, and 0x50 later.  It is
 * cast to void below rather than quietly renamed, because a reader who found
 * a `V90SpectralVerifier *` parameter and no use of it should be told that is
 * the object's doing.  `V90Demodulator` passes the address of its own
 * embedded verifier, which is what makes the argument look load-bearing from
 * the caller's side.
 *
 * `word_3cc` IS IN THE INITIALIZER LIST AND NOT THE BODY, by finding 1302's
 * argument: `mov %ecx,0x3cc(%ebx)` with `%ecx` zero sits between the two
 * member constructor calls and could not have been moved across either.
 *
 * THE FOUR SD-DETECTOR ARGUMENTS ARE RELOADED FROM THE MEMBER, NOT THE
 * PARAMETER.  `mov 0xc(%ebx),%eax` appears four times, once before each of
 * +0x284, +0x288, +0x28c and +0x290 -- so the source reads `params->...`
 * where `params` is `this->params`, which the line above has just stored, and
 * not the argument still sitting in a register.  Three of the four are copied
 * with `mov` and never loaded onto the x87 stack, so they are floats copied
 * bit-exactly rather than converted; the fourth is the `unsigned` limit.
 */
V90Phase3Demodulator::V90Phase3Demodulator(V90Parameters *p,
					   V90SpectralVerifier *unused,
					   unsigned int flag,
					   V90AutoDigitalImpDetector *adid)
	: phase3Modulator(p, flag), word_3cc(),
	  descrambler(P3D_DSC_TAP1, P3D_DSC_TAIL, P3D_DSC_OUT)
{
	V90SdDetector *sd;
	ANSamToneDetector *an;

	(void)unused;

	params = p;
	sessionFlag = flag;
	autoDigitalImpDetector = adid;

	sd = (V90SdDetector *)sysdep_malloc(sizeof(V90SdDetector));
	v90p3d_sd_ctor(sd, V90PF(params)[PARAMS_SD_THRESH_08],
		       V90PF(params)[PARAMS_SD_THRESH_0C],
		       V90PF(params)[PARAMS_SD_VALUE_10],
		       (unsigned int)V90PW(params)[PARAMS_SD_LIMIT]);
	sdDetector = sd;

	an = (ANSamToneDetector *)sysdep_malloc(sizeof(ANSamToneDetector));
	v90p3d_ansam_ctor(an, P3D_ANSAM_SAMPLES1, P3D_ANSAM_SAMPLES2,
			  P3D_ANSAM_THRESHOLD, 0, P3D_ANSAM_RATIO,
			  P3D_ANSAM_SAMPLE_RATE, P3D_ANSAM_BLOCK_LEN,
			  P3D_ANSAM_BLOCK_SIZE);
	ansamToneDetector = an;

	reset((PcmType)0, 0x40, (Phase3DemodulatorState)0, 0, 0, 0, 0, 0, 1,
	      0.0f, 0);
}

/*
 * `~V90Phase3Demodulator` -- 165 bytes at 0x20cb0 (D1) and 0x20c00 (D2).
 *
 * Two guarded heap arms and then two calls this file must NOT write: the
 * compiler emits `descrambler`'s and `phase3Modulator`'s destruction after
 * the body, in reverse declaration order, and both are visible at 0x20c2c
 * and 0x20c37 in the object.
 *
 * THE NULL TESTS ARE NOT DECORATION.  This tree's `sysdep_free` tolerates
 * NULL, so dropping either `if` leaves every byte comparison unchanged; what
 * moves is `harness_alloc.free_null`, which is what the test asserts.  The
 * SD detector is freed FIRST, which is the reverse of nothing -- the two
 * allocations are independent -- but it is the object's order and is
 * reproduced.
 */
V90Phase3Demodulator::~V90Phase3Demodulator()
{
	if (sdDetector) {
		sdDetector->~V90SdDetector();
		sysdep_free(sdDetector);
	}
	if (ansamToneDetector) {
		ansamToneDetector->~ANSamToneDetector();
		sysdep_free(ansamToneDetector);
	}
}

/*
 * ===========================================================================
 * THE TWO DECISION FUNCTIONS, AND THE IDIOMS THEY SHARE.
 *
 * `getV90Decision` (0x23830, 8,379 bytes) and `getV92Decision` (0x21680,
 * 8,616 bytes) were reconstructed independently and land here together.  They
 * are two thirty-four-arm state machines over the same object, so they reach
 * for the same handful of open-coded idioms, and those are defined ONCE below
 * rather than twice -- a macro redefined with different replacement text is
 * ill-formed, and a macro redefined with the SAME text but a different
 * measured justification is worse, because only one of the two justifications
 * can be true.
 *
 * THREE NAMES WERE SPELLED DIFFERENTLY BY THE TWO RECONSTRUCTIONS and all
 * three are settled here in favour of the spelling that was MEASURED against
 * the object rather than asserted.  They agree over every value either
 * function can hand them -- see finding 2116 -- so this is a codegen question
 * and never a behavioural one:
 *
 *   P3D_CODE   `getV92Decision` wrote `(int)(unsigned char)`, which is the
 *              obvious reading; `getV90Decision` measured the object and found
 *              a sixteen-bit result at the sixteen sites that index a table.
 *              The wider spelling wins and the note below is its evidence.
 *              Both give 0..255 for every input, and an `unsigned short` in
 *              that range promotes to the same `int`, so no site changes value.
 *   P3D_SIGN   the same shift; `getV90Decision` adds an explicit `(int)` cast
 *              that is a no-op at all forty-three sites (the arguments are a
 *              `short` and an `int`, never an unsigned type, so the shift is
 *              arithmetic either way).  The cast is kept for being explicit.
 *   P3D_ABS    `getV92Decision` wrote the conditional and `getV90Decision` the
 *              builtin, and the two comments made DIRECTLY CONTRADICTORY
 *              claims about what GCC 3.4.2 does with the conditional.  That
 *              disagreement was settled by measurement, not by preference --
 *              finding 2117.
 * ===========================================================================
 */

/*
 * The magnitude-to-code conversion, open-coded at all twenty of its sites in
 * the blob.  A macro rather than a helper because a helper would have to be
 * relied on to inline, and at -O2 GCC 3.4.2 inlines nothing that is not
 * declared `inline`.  A-law is the NON-zero arm, as in `reset` above.
 *
 * SIXTEEN BITS WIDE, AND `make similarity` IS WHAT SETTLES THAT.  Written as
 * `unsigned char` the u-law arm comes out as `not %al` at all twenty-two
 * sites; the object has `not %al` at the six that feed an `unsigned char`
 * argument and `movzbw %al,%cx; sub %ecx,%ebp; movzwl %bp,%eax` at the
 * sixteen that index a table -- a truncation to sixteen bits that is
 * unobservable, because the value is 0..255, and that the compiler therefore
 * emitted only because the type asked for it.  Widening the macro's result
 * puts all twenty-two back.  It costs about sixty instructions elsewhere in
 * register allocation, which is the half of a codegen difference CLAUDE.md
 * says to ignore.
 *
 * EVERY COUNT IN THE TWO PARAGRAPHS ABOVE IS `getV90Decision`'s ALONE -- its
 * twenty open-coded sites, its twenty-two `not %al`, its six and its sixteen.
 * They were measured before `getV92Decision` landed beside it and they have
 * NOT been re-derived over the pair, which between them use this macro
 * forty-four times.  The conclusion is unaffected, because it is about what
 * the object encodes at sites this function owns; the numbers are not
 * totals for the file and must not be quoted as any.
 */
#define P3D_CODE(mag)							\
	((unsigned short)(pcmType == PCM_TYPE_MU_LAW			\
			  ? 0xff - linear2ulaw(mag)			\
			  : linear2alaw(mag) ^ 0xd5))

/*
 * -1 for a negative sample, +1 otherwise, and SPELLED AS THE SHIFT because the
 * object is `sar $0x1f; or $0x1`.  GCC 3.4.2 compiles `x < 0 ? -1 : 1` to a
 * test and a branch, so the two spellings are not interchangeable here even
 * though they agree over every input.
 */
#define P3D_SIGN(x)	((((int)(x)) >> 31) | 1)

/*
 * `cltd; xor %edx,%eax; sub %edx,%eax`, which is abs() and not a conditional
 * negation -- GCC 3.4.2 emits a branch for `x < 0 ? -x : x` and this sequence
 * for the builtin.  V90Equalizer.cpp reaches for the same builtin for the same
 * reason, and V90Phase2Info.cpp's comment names the instruction triple.
 */
#define P3D_ABS(x)	__builtin_abs(x)

/*
 * The phase index is truncated to sixteen bits at every table lookup
 * (`movzwl 0x4(%ebx)`) even though +0x04 is a 32-bit field.  It only ever
 * holds 0..5 so the truncation cannot be observed, but it is in the object.
 */
#define P3D_PHASE	((unsigned short)word_04)

/*
 * The detector's two per-phase mapping tables and its one-dimensional
 * previous-frame table.  The object indexes all three FLAT -- `(fp << 7) + c`
 * with `c` up to 255 -- so a row index is not a bound here, and `prevLinMapp`
 * is read UNSIGNED (`movzwl`) where the other two are read signed (`movswl`).
 */
#define P3D_LINMAPP(a, c)	((a)->linMapp[(unsigned short)word_04][(c)])
#define P3D_LINMAPPALT(a, c)	((a)->linMappAlt[(unsigned short)word_04][(c)])
#define P3D_PREVLINMAPP(a, c) \
	(((const unsigned short *)(const void *)&(a)->prevLinMapp[0])[(c)])

/* The parameter block, by index, for the reason the constructor above uses. */
#define P3D_P_PROBING_MODE	(0x004 / 4)
#define P3D_P_DFE_LENGTH	(0x1fc / 4)
#define P3D_P_TRN1D_DD_LENGTH	(0x2fc / 4)
#define P3D_P_300		(0x300 / 4)
#define P3D_P_308		(0x308 / 4)
#define P3D_P_30C		(0x30c / 4)
#define P3D_P_310		(0x310 / 4)
#define P3D_P_314		(0x314 / 4)
#define P3D_P_318		(0x318 / 4)
#define P3D_P_31C		(0x31c / 4)
#define P3D_P_320		(0x320 / 4)
#define P3D_P_344		(0x344 / 4)
#define P3D_P_438		(0x438 / 4)
#define P3D_P_440		(0x440 / 4)
#define P3D_P_TRN1_QC_DD	(0x4a0 / 4)
#define P3D_P_4A4		(0x4a4 / 4)
#define P3D_P_ANSPCM_LENGTH	(0x4cc / 4)

/*
 * `params->unnamed_438 = params->unnamed_440` is `mov 0x440(%c),%eax;
 * mov %eax,0x438(%c)` -- a raw 32-bit copy.  This comment used to add "between
 * a slot this tree types `float` and one it types `int`", which was the reason
 * for the word view; finding 2112 has since retyped +0x440 to `float` on the
 * strength of exactly this instruction pair, so BOTH slots are `float` now and
 * `getV90Decision` spells the same copy as a plain float assignment.  The word
 * view is kept here because it is what the object encodes and because it is
 * what this function was tested with; the two are the same four bytes.
 */
#define P3D_COPY_440_TO_438() \
	(V90PW(params)[P3D_P_438] = V90PW(params)[P3D_P_440])

/* `word_04` runs 0,1,2,3,4,5,0 -- `inc; cmp $6; je; mov`. */
#define P3D_BUMP_FRAME() \
	do { \
		unsigned int next_ = word_04 + 1; \
		if (next_ == 6) \
			word_04 = 0; \
		else \
			word_04 = next_; \
	} while (0)

/* The shared tail of the DIL arms, folded by the compiler into one block. */
#define P3D_CHECK_TERMINATED() \
	do { \
		if (phase3Modulator.eventCode == 6) { \
			edprintf("V90Phase3Demodulator: Phase3 Terminated @ %d\r\n", word_2c); \
			state = (Phase3DemodulatorState)0x13; \
			word_2c = 0; \
			word_30 = 0x14; \
		} \
	} while (0)

/*
 * One data bit, and the level that goes with it.  The alternate mapping table
 * is consulted only when the detector's +0xa948 flag is set AND `isAltRbs`
 * agrees, which is the object's `jne`-then-`test %ax,%ax` pair; `isAltRbs` is
 * declared `int` in V90AutoDigitalImpDetector.h and TESTED SIXTEEN BITS WIDE
 * here, so the narrowing is the object's and not decoration.
 */
#define P3D_DEMOD_BIT() \
	do { \
		V90AutoDigitalImpDetector *ad_ = autoDigitalImpDetector; \
		int v_; \
		\
		if (ad_->short_a948 != 0 && \
		    (short)ad_->isAltRbs((short)word_04, ucode, sample) != 0) \
			v_ = P3D_LINMAPPALT(ad_, ucode); \
		else \
			v_ = P3D_LINMAPP(ad_, ucode); \
		if (sample > 0.0f) { \
			bit = 1; \
			level = v_; \
		} else { \
			bit = 0; \
			level = (short)-v_; \
		} \
		bit = word_3cc.process(bit); \
		bit = descrambler.process(bit); \
		decision = (short)level; \
	} while (0)

/*
 * ===========================================================================
 * `getV90Decision(float)` -- 8,379 bytes at 0x23830, and the receiver's whole
 * phase 3 state machine.  docs/v90p3ddecision.md is the decode; this comment
 * is only what a reader of the code below needs in front of them.
 *
 * THE RETURN TYPE IS `short`.  `getDecision` sign-extends `%ax` with `cwtl`
 * after calling this, which it would not need for an `int`.
 *
 * `decision` IS DELIBERATELY UNINITIALISED.  The dispatch's default block at
 * 0x238b0 falls straight into `mov %edi,%eax` without ever writing `%edi`, so
 * states 7, 8, 0x12 and everything above 0x21 return whatever the caller left
 * in that callee-saved register.  Initialising it here would be a different
 * program.  D321.
 *
 * `word_30 = 0` IS PER CASE AND NOT HOISTED.  Case 0 is the proof: it has no
 * store at the top of its block and reaches one only as the `else` of the
 * `if` that stores 9.
 *
 * FOUR COPIES OF `twoLevelDemod`.  Cases 4, 5, 6 and 9 open with a block that
 * is the body of `V90Phase3Demodulator::twoLevelDemod(float, int &)` -- its
 * own GLOBAL blob symbol at 0x215a0, so not `inline` and so not something GCC
 * 3.4.2 would have inlined at -O2.  The duplication is the author's.
 * ===========================================================================
 */

short
V90Phase3Demodulator::getV90Decision(float sample)
{
	short decision;
	short s;
	short symbol;
	short level;
	int bit;

	s = (short)sample;
	word_2c++;

	switch ((int)state) {

	/* ------------------------------------------------ 0x00 WaitForSd */
	case 0x00:
		decision = s;
		if (sdDetector->count == 10)
			byte_424 = 1;
		if (byte_424 != 0 && sdDetector->count == 0) {
			byte_424 = 0;
			word_30 = 9;
		} else {
			word_30 = 0;
		}
		if (sdDetector->process(sample) > 0) {
			edprintf("V90Phase3Demodulator: Sd detected @ %d\r\n",
				 word_2c);
			state = (Phase3DemodulatorState)0x01;
			word_2c = 0;
			word_30 = 1;
		} else if (word_2c == word_14 + 12000.0f) {
			state = (Phase3DemodulatorState)0x14;
			word_2c = 0;
			word_30 = 0x15;
			edprintf("V90Phase3Demodulator: WaitForSd TimeOut\r\n");
		}
		break;

	/* -------------------------------------------------- 0x01 SdDemod */
	case 0x01:
		word_30 = 0;
		decision = s;
		if (sdDetector->process(sample) < 0) {
			edprintf("V90Phase3Demodulator: SdNot detected @ %d\r\n",
				 word_2c);
			state = (Phase3DemodulatorState)0x02;
			word_2c = 0;
			word_30 = 2;
		} else if (word_2c == 0x180) {
			state = (Phase3DemodulatorState)0x15;
			word_2c = 0;
			word_30 = 0x15;
			edprintf("V90Phase3Demodulator: SdDemod TimeOut\r\n");
		}
		break;

	/* ------------------------------------- 0x02 SdNot seen, settle in */
	case 0x02:
		word_30 = 0;
		decision = s;
		if (word_2c == 0x30) {
			if (word_410 != 0) {
				state = (Phase3DemodulatorState)0x04;
				word_420 = (unsigned int)
				    params->TRN1_QC_DD_LENGTH;
				word_3f4 = (unsigned int)params->unnamed_4a4;
				/*
				 * NO NULL TEST, unlike case 3 which diagnoses
				 * "ERROR: Null JdDetector" for the same
				 * pointer.  D322.
				 */
				jd->unPackReset();
				edprintf("V90Phase3Demodulator: enter TRN1d "
					 "DD state\r\n");
			} else {
				state = (Phase3DemodulatorState)0x03;
				edprintf("V90Phase3Demodulator: enter TRN1d "
					 "Known Data state\r\n");
			}
			autoDigitalImpDetector->resetStudyUrefHandler(word_410);
			word_2c = 0;
			word_30 = 3;
		}
		break;

	/* ------------------------------------------ 0x03 TRN1dKnownData */
	case 0x03:
		word_30 = 0;
		decision = (short)phase3Modulator.generateSymbol();
		if (word_2c == 0x7f8) {
			if (jd == NULL) {
				state = (Phase3DemodulatorState)0x19;
				word_30 = 0x15;
				if (DSPLIB_DEBUG_ON())
					dsplibs_debug_printf(
					    "V90Phase3Demodulator: ERROR: "
					    "Null JdDetector\r\n");
			} else {
				state = (Phase3DemodulatorState)0x04;
				if (word_410 != 0) {
					if (DSPLIB_DEBUG_ON())
						dsplibs_debug_printf(
						    "V90Phase3Demodulator: "
						    "setting params for short "
						    "TRN1\n");
					word_420 = (unsigned int)
					    params->TRN1_QC_DD_LENGTH;
					word_3f4 = (unsigned int)
					    params->unnamed_4a4;
				} else {
					word_420 = (unsigned int)
					    params->TRN1D_DD_LENGTH;
					word_3f4 = (unsigned int)
					    params->unnamed_344;
				}
				autoDigitalImpDetector->resetStudyUrefHandler(
				    word_410);
				jd->unPackReset();
				edprintf("V90Phase3Demodulator: enter TRN1d "
					 "DD state\r\n");
			}
			word_2c = 0;
		}
		break;

	/* ----------------------------------------------- 0x04 TRN1d DD */
	case 0x04:
		word_30 = 0;
		/* twoLevelDemod(sample, bit), copy 1 of 4 */
		if (autoDigitalImpDetector->short_a948 != 0
		    && autoDigitalImpDetector->isAltRbs((short)word_04, ucode,
							sample))
			level = autoDigitalImpDetector
			    ->linMappAlt[P3D_PHASE][ucode];
		else
			level = autoDigitalImpDetector
			    ->linMapp[P3D_PHASE][ucode];
		if (sample > 0.0f) {
			bit = 1;
		} else {
			bit = 0;
			level = (short)-level;
		}
		bit = word_3cc.process(bit);
		bit = descrambler.process(bit);
		decision = level;

		if (word_2c == word_420) {
			edprintf("V90Phase3Demodulator: enter study reference "
				 "Ucode state @ %d\r\n", word_2c);
			state = (Phase3DemodulatorState)0x05;
			word_2c = 0;
			word_30 = 4;
		} else if (word_2c == 36000.0f) {
			state = (Phase3DemodulatorState)0x16;
			word_2c = 0;
			word_30 = 0x15;
			edprintf("V90Phase3Demodulator: TRN1dDemod "
				 "TimeOut\r\n");
		}
		break;

	/* ------------------------------------ 0x05 study reference Ucode */
	case 0x05:
		word_30 = 0;
		/* twoLevelDemod(sample, bit), copy 2 of 4 */
		if (autoDigitalImpDetector->short_a948 != 0
		    && autoDigitalImpDetector->isAltRbs((short)word_04, ucode,
							sample))
			level = autoDigitalImpDetector
			    ->linMappAlt[P3D_PHASE][ucode];
		else
			level = autoDigitalImpDetector
			    ->linMapp[P3D_PHASE][ucode];
		if (sample > 0.0f) {
			bit = 1;
		} else {
			bit = 0;
			level = (short)-level;
		}
		bit = word_3cc.process(bit);
		bit = descrambler.process(bit);
		decision = level;

		word_408 = (unsigned int)
		    autoDigitalImpDetector->studyUrefHandler(sample, word_04);
		if (++word_04 == 6)
			word_04 = 0;
		if (word_408 == 2) {
			if (word_2c % 6 == 0) {
				if (autoDigitalImpDetector
				    ->isThereAnyAltRbsPhase())
					params
					    ->PHASE4_MEAN_ERROR_BEF_TO_AFT_UPDATE_RATIO_THRESH
					    = params->unnamed_440;
				edprintf("V90Phase3Demodulator: enter Wait "
					 "For Jd state @ %d\r\n", word_2c);
				state = (Phase3DemodulatorState)0x06;
				word_2c = 0;
				word_30 = 5;
			}
			word_408 = 1;
			break;
		}
		if (word_2c == 36000.0f) {
			state = (Phase3DemodulatorState)0x16;
			word_2c = 0;
			word_30 = 0x15;
			edprintf("V90Phase3Demodulator: TRN1dDemod "
				 "TimeOut\r\n");
		}
		break;

	/* ----------------------------------------------- 0x06 WaitForJd */
	case 0x06:
		word_30 = 0;
		/* twoLevelDemod(sample, bit), copy 3 of 4 */
		if (autoDigitalImpDetector->short_a948 != 0
		    && autoDigitalImpDetector->isAltRbs((short)word_04, ucode,
							sample))
			level = autoDigitalImpDetector
			    ->linMappAlt[P3D_PHASE][ucode];
		else
			level = autoDigitalImpDetector
			    ->linMapp[P3D_PHASE][ucode];
		if (sample > 0.0f) {
			bit = 1;
		} else {
			bit = 0;
			level = (short)-level;
		}
		bit = word_3cc.process(bit);
		bit = descrambler.process(bit);
		decision = level;

		if (++word_04 == 6)
			word_04 = 0;
		if (jd->unPackData(bit)) {
			if (word_2c % 6 == 0 || word_410 != 0) {
				if (DSPLIB_DEBUG_ON())
					dsplibs_debug_printf(
					    "V90Phase3Demodulator: waitForJd "
					    "framePosition = %d\n", word_04);
				if (word_04 != 0) {
					if (DSPLIB_DEBUG_ON())
						dsplibs_debug_printf(
						    "V90Phase3Demodulator: "
						    "adjustUinfoToPhaseOffset"
						    "\n");
					autoDigitalImpDetector
					    ->adjustUinfoToPhaseOffset(
						(short)word_04);
					word_04 = 0;
				}
				edprintf("V90Phase3Demodulator: Jd detected "
					 "@ %d\r\n", word_2c);
				word_30 = 6;
				state = (Phase3DemodulatorState)0x09;
				word_2c = 0;
				word_404 = 0;
			} else {
				jd->unPackReset();
			}
		}
		if (word_2c == 36000.0f) {
			state = (Phase3DemodulatorState)0x16;
			word_2c = 0;
			word_30 = 0x15;
			edprintf("V90Phase3Demodulator: TRN1dDemod "
				 "TimeOut\r\n");
		}
		break;

	/* ------------------------------------------------- 0x09 JdDemod */
	case 0x09:
		word_30 = 0;
		/* twoLevelDemod(sample, bit), copy 4 of 4 */
		if (autoDigitalImpDetector->short_a948 != 0
		    && autoDigitalImpDetector->isAltRbs((short)word_04, ucode,
							sample))
			level = autoDigitalImpDetector
			    ->linMappAlt[P3D_PHASE][ucode];
		else
			level = autoDigitalImpDetector
			    ->linMapp[P3D_PHASE][ucode];
		if (sample > 0.0f) {
			bit = 1;
		} else {
			bit = 0;
			level = (short)-level;
		}
		bit = word_3cc.process(bit);
		bit = descrambler.process(bit);
		decision = level;

		if (++word_04 == 6)
			word_04 = 0;
		if (bit != 0)
			word_404 = 0;
		else
			word_404++;
		if (word_404 > 0xb && word_2c % 72 == 12) {
			edprintf("V90Phase3Demodulator: JdNot detected @ %d\r\n",
				 word_2c);
			word_30 = 8;
			if (word_410 != 0) {
				state = (Phase3DemodulatorState)0x0d;
				if (DSPLIB_DEBUG_ON())
					dsplibs_debug_printf(
					    "V90Phase3Demodulator: changing "
					    "state to DILDemodQCfirstStudy\n");
			} else {
				state = (Phase3DemodulatorState)
				    (params->PROBING_MODE ? 0x11 : 0x0a);
			}
			word_2c = 0;
			phase3Modulator.reset(pcmType, ucode, P3M_STATE_DIL, 0,
					      NULL, NULL, dil, 0);
			break;
		}
		if (word_2c == word_14 + 38760.0f) {
			state = (Phase3DemodulatorState)0x17;
			word_2c = 0;
			word_30 = 0x15;
			edprintf("V90Phase3Demodulator: JdDemod TimeOut\r\n");
		}
		break;

	/* -------------------------------------- 0x0a DILDemodFirstStudy */
	case 0x0a:
		word_30 = 0;
		decision = s;
		symbol = (short)phase3Modulator.generateSymbol();
		if (phase3Modulator.usingSegmentLevel != 0) {
			if (autoDigitalImpDetector->isAltRbs((short)word_04,
							     ucode, sample))
				decision = (short)(P3D_SIGN(s)
				    * autoDigitalImpDetector
				      ->linMappAlt[P3D_PHASE]
						  [P3D_CODE(P3D_ABS((int)symbol))]);
			else
				decision = (short)(P3D_SIGN(s)
				    * autoDigitalImpDetector
				      ->linMapp[P3D_PHASE]
					       [P3D_CODE(P3D_ABS((int)symbol))]);
		}
		word_408 = 1;
		autoDigitalImpDetector->calculateLinearMeanAndVar(s, symbol,
								  word_04);
		if (++word_04 == 6)
			word_04 = 0;
		if (word_2c == (unsigned int)params->unnamed_30c) {
			autoDigitalImpDetector->porcessFirstStudy();
			if (autoDigitalImpDetector->isThereAnyAltRbsPhase())
				params
				    ->PHASE4_MEAN_ERROR_BEF_TO_AFT_UPDATE_RATIO_THRESH
				    = params->unnamed_440;
			word_2c = 0;
			state = (Phase3DemodulatorState)0x0b;
		} else if (word_2c == 0x9c40) {
			state = (Phase3DemodulatorState)0x18;
			word_2c = 0;
			word_30 = 0x15;
			edprintf("V90Phase3Demodulator: DILDemodFirstStudy "
				 "TimeOut\r\n");
		}
		if (phase3Modulator.eventCode == 6) {
			edprintf("V90Phase3Demodulator: Phase3 Terminated "
				 "@ %d\r\n", word_2c);
			state = (Phase3DemodulatorState)0x13;
			word_2c = 0;
			word_30 = 0x14;
		}
		break;

	/* ------------------------------------- 0x0b DILDemodSecondStudy */
	case 0x0b:
		word_30 = 0;
		symbol = (short)phase3Modulator.generateSymbol();
		autoDigitalImpDetector->calculateLinearMeanAndVar(s, symbol,
								  word_04);
		if (autoDigitalImpDetector->short_2800[word_04] != 0) {
			if (P3D_ABS((int)symbol) == ucodeLevel) {
				if (autoDigitalImpDetector->isAltRbs(
				    (short)word_04, ucode, sample))
					decision = (short)(P3D_SIGN(s)
					    * autoDigitalImpDetector
					      ->linMappAlt[P3D_PHASE]
					        [P3D_CODE(P3D_ABS((int)symbol))]);
				else
					decision = (short)(P3D_SIGN(s)
					    * autoDigitalImpDetector
					      ->linMapp[P3D_PHASE]
					        [P3D_CODE(P3D_ABS((int)symbol))]);
			} else {
				decision = s;
				autoDigitalImpDetector
				    ->addReceivedSampleToStorage(
					(short)word_04,
					P3D_CODE(P3D_ABS((int)symbol)),
					sample);
			}
		} else {
			if (P3D_ABS((int)symbol) != ucodeLevel)
				autoDigitalImpDetector
				    ->updateLinMappMeanAndVar((short)word_04,
					P3D_CODE(P3D_ABS((int)symbol)));
			decision = (short)(P3D_SIGN(s)
			    * autoDigitalImpDetector
			      ->linMapp[P3D_PHASE]
				       [P3D_CODE(P3D_ABS((int)symbol))]);
		}
		word_408 = 1;
		if (phase3Modulator.segmentPos == 0
		    && ucode != phase3Modulator.dilPcmCode)
			autoDigitalImpDetector
			    ->uniteLinMappInfoOfUnsuspectedPhases(
				phase3Modulator.dilPcmCode);
		if (word_2c == (unsigned int)params->unnamed_314)
			word_30 = 0x0c;
		if (word_2c == (unsigned int)params->unnamed_300) {
			if (short_400 != 0) {
				word_30 = 0x10;
				params->unnamed_31c = params->unnamed_320;
			} else {
				word_30 = 0x0a;
			}
		}
		if (++word_04 == 6)
			word_04 = 0;
		if (word_2c == (unsigned int)params->unnamed_308) {
			if (params->unnamed_310 != 0) {
				word_30 = 0x10;
				state = (Phase3DemodulatorState)0x0c;
			} else {
				autoDigitalImpDetector->porcessSecondStudy();
				word_30 = 0x11;
				state = (Phase3DemodulatorState)0x10;
			}
			word_2c = 0;
		} else if (word_2c == 0x9c40) {
			state = (Phase3DemodulatorState)0x18;
			word_2c = 0;
			word_30 = 0x15;
			edprintf("V90Phase3Demodulator: DILDemodSecondStudy "
				 "TimeOut\r\n");
		}
		if (phase3Modulator.eventCode == 6) {
			edprintf("V90Phase3Demodulator: Phase3 Terminated "
				 "@ %d\r\n", word_2c);
			state = (Phase3DemodulatorState)0x13;
			word_2c = 0;
			word_30 = 0x14;
		}
		break;

	/* ---------------------------------- 0x0c DILDemodThirdStudyStage */
	case 0x0c:
		word_30 = 0;
		symbol = (short)phase3Modulator.generateSymbol();
		if (autoDigitalImpDetector->short_2800[word_04] != 0) {
			decision = s;
			if (phase3Modulator.usingSegmentLevel == 0)
				autoDigitalImpDetector
				    ->addReceivedSampleToStorage(
					(short)word_04,
					P3D_CODE(P3D_ABS((int)symbol)),
					sample);
		} else {
			if (phase3Modulator.usingSegmentLevel == 0) {
				autoDigitalImpDetector
				    ->calculateLinearMeanAndVar(s, symbol,
								word_04);
				autoDigitalImpDetector
				    ->updateLinMappMeanAndVar((short)word_04,
					P3D_CODE(P3D_ABS((int)symbol)));
			}
			decision = (short)(P3D_SIGN(s)
			    * autoDigitalImpDetector
			      ->linMapp[P3D_PHASE]
				       [P3D_CODE(P3D_ABS((int)symbol))]);
		}
		word_408 = 0;
		if (++word_04 == 6)
			word_04 = 0;
		if (phase3Modulator.segmentPos == 0
		    && ucode != phase3Modulator.dilPcmCode)
			autoDigitalImpDetector
			    ->uniteLinMappInfoOfUnsuspectedPhases(
				phase3Modulator.dilPcmCode);
		if (word_2c == (unsigned int)params->unnamed_310) {
			autoDigitalImpDetector->porcessSecondStudy();
			word_2c = 0;
			word_30 = 0x11;
			state = (Phase3DemodulatorState)0x10;
		} else if (word_2c == 0x9c40) {
			state = (Phase3DemodulatorState)0x18;
			word_2c = 0;
			word_30 = 0x15;
			edprintf("V90Phase3Demodulator: "
				 "DILDemodThirdStudyStage TimeOut\r\n");
		}
		if (phase3Modulator.eventCode == 6) {
			edprintf("V90Phase3Demodulator: Phase3 Terminated "
				 "@ %d\r\n", word_2c);
			state = (Phase3DemodulatorState)0x13;
			word_2c = 0;
			word_30 = 0x14;
		}
		break;

	/* ------------------------------------ 0x0d DILDemodQCfirstStudy */
	case 0x0d:
		word_30 = 0;
		decision = s;
		symbol = (short)phase3Modulator.generateSymbol();
		if (phase3Modulator.usingSegmentLevel != 0) {
			if (autoDigitalImpDetector->isAltRbs((short)word_04,
							     ucode, sample))
				decision = (short)(P3D_SIGN(s)
				    * autoDigitalImpDetector
				      ->linMappAlt[P3D_PHASE]
						  [P3D_CODE(P3D_ABS((int)symbol))]);
			else
				decision = (short)(P3D_SIGN(s)
				    * autoDigitalImpDetector
				      ->linMapp[P3D_PHASE]
					       [P3D_CODE(P3D_ABS((int)symbol))]);
		}
		word_408 = 1;
		autoDigitalImpDetector->calculateLinearMeanAndVar(s, symbol,
								  word_04);
		if (++word_04 == 6)
			word_04 = 0;
		if (word_2c == (unsigned int)params->unnamed_30c) {
			autoDigitalImpDetector->porcessFirstStudy();
			if (autoDigitalImpDetector->isThereAnyAltRbsPhase())
				params
				    ->PHASE4_MEAN_ERROR_BEF_TO_AFT_UPDATE_RATIO_THRESH
				    = params->unnamed_440;
			word_2c = 0;
			state = (Phase3DemodulatorState)0x0e;
		} else if (word_2c == 0x9c40) {
			state = (Phase3DemodulatorState)0x18;
			word_2c = 0;
			word_30 = 0x15;
			edprintf("V90Phase3Demodulator: DILDemodQCfirstStudy "
				 "TimeOut\r\n");
		}
		if (phase3Modulator.eventCode == 6) {
			edprintf("V90Phase3Demodulator: Phase3 Terminated "
				 "@ %d\r\n", word_2c);
			state = (Phase3DemodulatorState)0x13;
			word_2c = 0;
			word_30 = 0x14;
		}
		break;

	/* ----------------------------------- 0x0e DILDemodQCsecondStudy */
	case 0x0e:
		word_30 = 0;
		symbol = (short)phase3Modulator.generateSymbol();
		autoDigitalImpDetector->calculateLinearMeanAndVar(s, symbol,
								  word_04);
		if (autoDigitalImpDetector->short_2800[word_04] != 0) {
			if (P3D_ABS((int)symbol) == ucodeLevel) {
				if (autoDigitalImpDetector->isAltRbs(
				    (short)word_04, ucode, sample))
					decision = (short)(P3D_SIGN(s)
					    * autoDigitalImpDetector
					      ->linMappAlt[P3D_PHASE]
					        [P3D_CODE(P3D_ABS((int)symbol))]);
				else
					decision = (short)(P3D_SIGN(s)
					    * autoDigitalImpDetector
					      ->linMapp[P3D_PHASE]
					        [P3D_CODE(P3D_ABS((int)symbol))]);
			} else {
				decision = s;
				autoDigitalImpDetector
				    ->addReceivedSampleToStorage(
					(short)word_04,
					P3D_CODE(P3D_ABS((int)symbol)),
					sample);
			}
		} else {
			if (P3D_ABS((int)symbol) == ucodeLevel) {
				decision = (short)(P3D_SIGN(s)
				    * autoDigitalImpDetector
				      ->linMapp[P3D_PHASE]
					       [P3D_CODE(P3D_ABS((int)symbol))]);
			} else {
				decision = s;
				if (autoDigitalImpDetector
				    ->byte_280c[word_04] == 0)
					decision = (short)(P3D_SIGN(s)
					    * autoDigitalImpDetector
					      ->prevLinMapp
					        [P3D_CODE(P3D_ABS((int)symbol))]);
			}
		}
		word_408 = 1;
		if (word_2c == (unsigned int)params->unnamed_314)
			word_30 = 0x0c;
		if (word_2c == (unsigned int)params->unnamed_300) {
			if (short_400 != 0) {
				word_30 = 0x10;
				params->unnamed_31c = params->unnamed_320;
			} else {
				word_30 = 0x0a;
			}
		}
		if (++word_04 == 6)
			word_04 = 0;
		if (word_2c == (unsigned int)params->unnamed_308) {
			word_2c = 0;
			word_30 = 0x10;
			state = (Phase3DemodulatorState)0x0f;
		} else if (word_2c == 0x9c40) {
			state = (Phase3DemodulatorState)0x18;
			word_2c = 0;
			word_30 = 0x15;
			edprintf("V90Phase3Demodulator: DILDemodQCsecondStudy "
				 "TimeOut\r\n");
		}
		if (phase3Modulator.eventCode == 6) {
			edprintf("V90Phase3Demodulator: Phase3 Terminated "
				 "@ %d\r\n", word_2c);
			state = (Phase3DemodulatorState)0x13;
			word_2c = 0;
			word_30 = 0x14;
		}
		break;

	/* ------------------------------------ 0x0f DILDemodQCthirdStudy */
	case 0x0f:
		word_30 = 0;
		symbol = (short)phase3Modulator.generateSymbol();
		if (phase3Modulator.usingSegmentLevel == 0)
			autoDigitalImpDetector->calculateLinearMeanAndVar(
			    s, symbol, word_04);
		if (autoDigitalImpDetector->short_2800[word_04] != 0) {
			decision = s;
			if (phase3Modulator.usingSegmentLevel == 0)
				autoDigitalImpDetector
				    ->addReceivedSampleToStorage(
					(short)word_04,
					P3D_CODE(P3D_ABS((int)symbol)),
					sample);
		} else {
			decision = s;
			if (autoDigitalImpDetector->byte_280c[word_04] == 0)
				decision = (short)(P3D_SIGN(s)
				    * autoDigitalImpDetector
				      ->prevLinMapp
					[P3D_CODE(P3D_ABS((int)symbol))]);
		}
		word_408 = 0;
		if (++word_04 == 6)
			word_04 = 0;
		if (word_2c == (unsigned int)params->unnamed_310) {
			autoDigitalImpDetector->setQcLinearMapping();
			word_2c = 0;
			word_30 = 0x11;
			state = (Phase3DemodulatorState)0x10;
		} else if (word_2c == 0x9c40) {
			state = (Phase3DemodulatorState)0x18;
			word_2c = 0;
			word_30 = 0x15;
			edprintf("V90Phase3Demodulator: DILDemodQCthirdStudy "
				 "TimeOut\r\n");
		}
		if (phase3Modulator.eventCode == 6) {
			edprintf("V90Phase3Demodulator: Phase3 Terminated "
				 "@ %d\r\n", word_2c);
			state = (Phase3DemodulatorState)0x13;
			word_2c = 0;
			word_30 = 0x14;
		}
		break;

	/* -------------------------------- 0x10 DILDemodErrorRelaxation */
	case 0x10:
		word_30 = 0;
		symbol = (short)phase3Modulator.generateSymbol();
		if (autoDigitalImpDetector->short_2800[word_04] != 0) {
			decision = s;
			if (phase3Modulator.usingSegmentLevel != 0) {
				if (autoDigitalImpDetector->isAltRbs(
				    (short)word_04, ucode, sample))
					decision = (short)(P3D_SIGN(s)
					    * autoDigitalImpDetector
					      ->linMappAlt[P3D_PHASE]
					        [P3D_CODE(P3D_ABS((int)symbol))]);
				else
					decision = (short)(P3D_SIGN(s)
					    * autoDigitalImpDetector
					      ->linMapp[P3D_PHASE]
					        [P3D_CODE(P3D_ABS((int)symbol))]);
			}
		} else {
			decision = (short)(P3D_SIGN(s)
			    * autoDigitalImpDetector
			      ->linMapp[P3D_PHASE]
				       [P3D_CODE(P3D_ABS((int)symbol))]);
		}
		if (++word_04 == 6)
			word_04 = 0;
		if (byte_3f9 != 0) {
			word_3fc++;
			if (word_3fc == (unsigned int)params->unnamed_31c) {
				word_30 = 0x0f;
				word_408 = 1;
			}
		}
		if (word_2c == (unsigned int)params->unnamed_318) {
			word_30 = 0x12;
		} else if (word_2c == 0x9c40) {
			state = (Phase3DemodulatorState)0x18;
			word_2c = 0;
			word_30 = 0x15;
			edprintf("V90Phase3Demodulator: "
				 "DILDemodErrorRelaxation TimeOut\r\n");
		}
		if (phase3Modulator.eventCode == 6) {
			edprintf("V90Phase3Demodulator: Phase3 Terminated "
				 "@ %d\r\n", word_2c);
			state = (Phase3DemodulatorState)0x13;
			word_2c = 0;
			word_30 = 0x14;
		}
		break;

	/* ------------------------------------------ 0x11 ProbingDILDemod */
	case 0x11:
		word_30 = 0;
		decision = s;
		symbol = (short)phase3Modulator.generateSymbol();
		if (P3D_ABS((int)symbol) == 0xffc
		    && phase3Modulator.segmentPos > (unsigned int)
		       params->DFE_LENGTH)
			word_408 = 1;
		else
			word_408 = 0;
		if (word_2c == dilLength) {
			edprintf("V90Phase3Demodulator: Probing DIL ended\r\n");
			word_30 = 0x13;
		} else if (word_2c == 0x9c40) {
			state = (Phase3DemodulatorState)0x18;
			word_2c = 0;
			word_30 = 0x15;
			edprintf("V90Phase3Demodulator: ProbingDILDemod "
				 "TimeOut\r\n");
		}
		break;

	/* ------------------------- 0x13..0x19 the terminal/parked states */
	case 0x13:
	case 0x14:
	case 0x15:
	case 0x16:
	case 0x17:
	case 0x18:
	case 0x19:
		word_30 = 0;
		decision = 0;
		break;

	/* ---------------------------------------------- 0x1a WaitForQts */
	case 0x1a:
		word_30 = 0;
		decision = s;
		ansamToneDetector->process(sample);
		if (sdDetector->process(sample) > 0) {
			edprintf("V90Phase3Demodulator: QTS detected @ %d\r\n",
				 word_2c);
			state = (Phase3DemodulatorState)0x1b;
			word_2c = 0;
			word_30 = 0x37;
		} else if (word_2c == (unsigned int)
			   params->ANSPCM_DEMODULATION_LENGTH) {
			state = (Phase3DemodulatorState)0x20;
			word_2c = 0;
			verificationStatus = 0;
			word_30 = 0x3a;
			edprintf("V90Phase3Demodulator: WaitFor Qts TimeOut, "
				 "decision is set to not same line\r\n");
		}
		break;

	/* ------------------------------------------- 0x1b WaitForQtsNot */
	case 0x1b:
		word_30 = 0;
		decision = s;
		ansamToneDetector->process(sample);
		if (sdDetector->process(sample) < 0) {
			edprintf("V90Phase3Demodulator: QTSNot detected "
				 "@ %d\r\n", word_2c);
			state = (Phase3DemodulatorState)0x1c;
			word_2c = 0;
			word_30 = 0x38;
		} else if (word_2c == 0x300) {
			state = (Phase3DemodulatorState)0x21;
			word_2c = 0;
			verificationStatus = 0;
			edprintf("V90Phase3Demodulator: WaitFor QtsNot "
				 "TimeOut, decision is set to not same "
				 "line\r\n");
			if (params->modemParams->sessionFlags & 1)
				params->ANSPCM_DEMODULATION_LENGTH = 0x320;
		}
		break;

	/* --------------------------------------- 0x1c enter ANSpcm demod */
	case 0x1c:
		word_30 = 0;
		decision = s;
		if (word_2c == 0x30) {
			state = (Phase3DemodulatorState)0x1d;
			word_2c = 0;
			if (params->modemParams->sessionFlags & 1)
				params->ANSPCM_DEMODULATION_LENGTH = 0x320;
			word_30 = 0x39;
			verificationStatus = 1;
			ansamToneDetector->reset();
			edprintf("V90Phase3Demodulator: enter ANSpcm demod "
				 "state\r\n");
		}
		break;

	/* --------------------------------------------- 0x1d ANSpcm demod */
	case 0x1d:
		word_30 = 0;
		decision = s;
		ansamToneDetector->process(sample);
		if (word_2c == (unsigned int)
		    params->ANSPCM_DEMODULATION_LENGTH)
			word_30 = 0x3a;
		break;

	/* --------------------------------- 0x1e wait for the energy drop */
	case 0x1e:
		word_30 = 0;
		decision = s;
		if (!ansamToneDetector->process(sample)) {
			if (DSPLIB_DEBUG_ON())
				dsplibs_debug_printf(
				    "V90Phase3Demodulator: ANSpcm energy drop "
				    "detected...\r\n");
			word_30 = 0x3b;
			state = (Phase3DemodulatorState)0x1f;
		}
		break;

	/* -------------------------------------------------- 0x1f parked */
	case 0x1f:
		word_30 = 0;
		decision = s;
		break;

	/* ------------------------------------------ 0x20 ANSpcm recovery */
	case 0x20:
		word_30 = 0;
		decision = s;
		if (ansamToneDetector->process(sample)) {
			edprintf("V90Phase3Demodulator: ANSpcm detected on "
				 "recovery, move to wait for drop...\r\n");
			word_30 = 0x3a;
		}
		if (word_2c > 0x7cf) {
			edprintf("V90Phase3Demodulator: faking ANSpcm energy "
				 "drop detected (QTs timeout)...\r\n");
			word_30 = 0x3b;
			state = (Phase3DemodulatorState)0x1f;
		}
		break;

	/* ------------------------------------- 0x21 ANSpcm recovery watch */
	case 0x21:
		word_30 = 0;
		decision = s;
		if (ansamToneDetector->process(sample)
		    && word_2c >= (unsigned int)
		       params->ANSPCM_DEMODULATION_LENGTH) {
			edprintf("V90Phase3Demodulator: ANSpcm detected on "
				 "recovery, move to wait for drop...\r\n");
			word_30 = 0x3a;
		}
		break;

	/*
	 * 7, 8, 0x12 and everything above 0x21 arrive here, and `decision` is
	 * never written on this path.  See the header comment.
	 */
	default:
		word_30 = 0;
		break;
	}

	return decision;
}

/*
 * `V90Phase3Demodulator::getV92Decision` -- 8,616 bytes at 0x21680, the
 * largest member of this class and the second largest unwritten function in
 * the object.
 *
 * ONE SAMPLE IN, ONE PCM DECISION OUT, AND A THIRTY-FOUR-WAY STATE MACHINE IN
 * BETWEEN.  The whole body is
 *
 *     word_2c++;  word_30 = 0;  switch (state) { ... }  return decision;
 *
 * dispatched through a jump table of 34 entries at `.rodata+0x7cc` guarded by
 * `cmp $0x21,%eax; ja`.  `word_2c` is the per-state sample counter -- every
 * arm that changes `state` also zeroes it -- and `word_30` is an event code
 * the caller reads, cleared on entry and set by whichever arm has news.
 *
 * THE RETURN TYPE IS `short` AND IT IS NOT A GUESS; see the declaration in
 * V90Phase3Demodulator.h for `getDecision`'s `cwtl`.
 *
 * AND ON THREE OF THE THIRTY-FOUR IT IS NOT WRITTEN AT ALL.  States 6 and 18
 * both point at the epilogue, as does the out-of-range default, and the
 * epilogue is `mov %edi,%eax` over an `%edi` no arm has touched -- so the
 * object returns whatever the caller left in that register.  That is a C
 * function with no `default:` and a declared-but-unassigned result, it is
 * reproduced by writing exactly that, and the differential test does not
 * compare the return value on those three states because there is nothing
 * there to compare.  docs/deviations.md, D-V92DEC-1.
 *
 * THE STATE NUMBERS ARE THE AUTHOR'S AND THIRTY OF THEM ARE NAMED, by the
 * diagnostics the arms emit.  They are spelled as casts rather than added to
 * `Phase3DemodulatorState`; the original reason was that `getV90Decision` was
 * being written against that enum concurrently and it must not move, and both
 * functions have now landed, so what is left is that `getV90Decision` above
 * casts too and the two would have to be enumerated together to agree.  That
 * is a clean separate step.  What the strings settle:
 *
 *      0 WaitForSd            1 SdDemod             2 SdNotDemod
 *      3 TRN1dKnownData       4 TRN1dDemod          5 study reference Ucode
 *      7 WaitForV92Jd         8 V92JdPhaseDemod     9 V92JdDemod
 *     10 DILDemodFirstStudy  11 DILDemodSecondStudy
 *     12 DILDemodThirdStudyStage                   13 DILDemodQCfirstStudy
 *     14 DILDemodQCsecondStudy                     15 DILDemodQCthirdStudy
 *     16 DILDemodErrorRelaxation                   17 ProbingDILDemod
 *     26 WaitForQts          27 WaitForQtsNot      28 (enter ANSpcm demod)
 *     29 ANSpcm demod        30/32/33 ANSpcm recovery
 *
 * FOUR IDIOMS CARRY MOST OF THE BYTES and are spelled as macros below so that
 * every arm shows its own shape rather than four hundred lines of repetition.
 * They are macros and not functions on purpose: the object has all of this
 * inlined into one body, and a `static` helper called eleven times is a
 * function GCC 3.4.2 need not inline.
 *
 *   P3D_DEMOD_BIT      the five arms that recover a data bit -- a table
 *                      lookup for the level, the sign of the sample for the
 *                      bit, then the serial differential decoder at +0x3cc
 *                      and the descrambler at +0x3d0.
 *   P3D_CODE           the companded index of a linear magnitude: A-law
 *                      `^ 0xd5`, mu-law `~`.  `pcmType == 0` selects mu-law,
 *                      which is the same sense `reset` uses.
 *   P3D_SIGN           `sar $0x1f; or $0x1` -- the sign of the raw level as
 *                      +1 or -1, which the mapping arms multiply by.
 *   P3D_CHECK_TERMINATED  the shared tail of the seven DIL arms, which the
 *                      compiler folded into one block at +0x4a6.
 *
 * THE THREE READ WIDTHS OF +0x04 are deliberate at every site; see the field's
 * comment in the header.  A cast that looks redundant here is the object's.
 */

short
V90Phase3Demodulator::getV92Decision(float sample)
{
	V90AutoDigitalImpDetector *adid;
	V90Phase3Modulator *mod;
	short decision;			/* %edi -- see the note above */
	int level = (short)sample;	/* %esi, the truncated sample     */
	int bit = 0;
	int sym;
	int code;

	word_2c++;
	word_30 = 0;

	switch ((int)state) {

	case 0:					/* WaitForSd */
		decision = (short)level;
		if (sdDetector->process(sample) > 0) {
			edprintf("V90Phase3Demodulator: Sd detected @ %d\r\n",
				 word_2c);
			state = (Phase3DemodulatorState)1;
			word_2c = 0;
			word_30 = 1;
		} else if (word_2c == word_14 + 12000.0f) {
			state = (Phase3DemodulatorState)0x14;
			word_2c = 0;
			word_30 = 0x15;
			edprintf("V90Phase3Demodulator: WaitForSd TimeOut\r\n");
		}
		break;

	case 1:					/* SdDemod */
		decision = (short)level;
		if (sdDetector->process(sample) < 0) {
			edprintf("V90Phase3Demodulator: SdNot detected "
				 "@ %d\r\n", word_2c);
			state = (Phase3DemodulatorState)2;
			word_2c = 0;
			word_30 = 2;
		} else if (word_2c == 0x180) {
			state = (Phase3DemodulatorState)0x15;
			word_2c = 0;
			word_30 = 0x15;
			edprintf("V90Phase3Demodulator: SdDemod TimeOut\r\n");
		}
		break;

	case 2:					/* SdNotDemod */
		decision = (short)level;
		if (word_2c == 0x30) {
			if (word_410 != 0) {
				state = (Phase3DemodulatorState)4;
				word_420 = (unsigned int)
				    V90PW(params)[P3D_P_TRN1_QC_DD];
				word_3f4 = (unsigned int)
				    V90PW(params)[P3D_P_4A4];
				jdV92->unPackJdReset();
				edprintf("V90Phase3Demodulator: enter TRN1d "
					 "DD state\r\n");
			} else {
				state = (Phase3DemodulatorState)3;
				edprintf("V90Phase3Demodulator: enter TRN1d "
					 "Known Data state\r\n");
			}
			autoDigitalImpDetector->resetStudyUrefHandler(word_410);
			word_2c = 0;
			word_30 = 3;
		}
		break;

	case 3:					/* TRN1dKnownData */
		decision = (short)phase3Modulator.generateSymbol();
		if (word_2c != 0x7f8)
			break;
		if (jdV92 == 0) {
			state = (Phase3DemodulatorState)0x19;
			word_30 = 0x15;
			if (DSPLIB_DEBUG_ON())
				dsplibs_debug_printf("V90Phase3Demodulator: "
				    "ERROR: Null V92JdDetector\r\n");
			word_2c = 0;
			break;
		}
		state = (Phase3DemodulatorState)4;
		if (word_410 != 0) {
			if (DSPLIB_DEBUG_ON())
				dsplibs_debug_printf("V90Phase3Demodulator: "
				    "V92 setting params for short TRN1\n");
			word_420 = (unsigned int)
			    V90PW(params)[P3D_P_TRN1_QC_DD];
			word_3f4 = (unsigned int)V90PW(params)[P3D_P_4A4];
		} else {
			word_420 = (unsigned int)
			    V90PW(params)[P3D_P_TRN1D_DD_LENGTH];
			word_3f4 = (unsigned int)V90PW(params)[P3D_P_344];
		}
		autoDigitalImpDetector->resetStudyUrefHandler(word_410);
		jdV92->unPackJdReset();
		edprintf("V90Phase3Demodulator: enter TRN1d DD state\r\n");
		word_2c = 0;
		break;

	case 4:					/* TRN1dDemod */
		P3D_DEMOD_BIT();
		if (word_2c == word_420) {
			edprintf("V90Phase3Demodulator: enter V92 study "
				 "reference Ucode state @ %d\r\n", word_2c);
			state = (Phase3DemodulatorState)5;
			word_2c = 0;
		} else if (word_2c == 36000.0f) {
			state = (Phase3DemodulatorState)0x16;
			word_2c = 0;
			word_30 = 0x15;
			edprintf("V90Phase3Demodulator: TRN1dDemod "
				 "TimeOut\r\n");
		}
		break;

	case 5:					/* study reference Ucode */
		P3D_DEMOD_BIT();
		adid = autoDigitalImpDetector;
		word_408 = (unsigned int)adid->studyUrefHandler(sample,
							       word_04);
		P3D_BUMP_FRAME();
		if (word_408 == 2) {
			if (word_2c % 6 == 0) {
				if ((short)adid->isThereAnyAltRbsPhase() != 0)
					P3D_COPY_440_TO_438();
				edprintf("V90Phase3Demodulator: JdNot detected "
					 "@ %d\r\n", word_2c);
				state = (Phase3DemodulatorState)7;
				word_2c = 0;
				word_30 = 5;
			}
			word_408 = 1;
		} else if (word_2c == 36000.0f) {
			state = (Phase3DemodulatorState)0x16;
			word_2c = 0;
			word_30 = 0x15;
			edprintf("V90Phase3Demodulator: TRN1dDemod "
				 "TimeOut\r\n");
		}
		break;

	case 7:					/* WaitForV92Jd */
		P3D_DEMOD_BIT();
		P3D_BUMP_FRAME();
		if ((unsigned char)jdV92->unPackJdData(bit) != 0) {
			if (word_2c % 6 == 0 || word_410 != 0) {
				if (DSPLIB_DEBUG_ON())
					dsplibs_debug_printf(
					    "V90Phase3Demodulator: waitForJd "
					    "framePosition = %d\n", word_04);
				if (word_04 != 0) {
					if (DSPLIB_DEBUG_ON())
						dsplibs_debug_printf(
						    "V90Phase3Demodulator: "
						    "adjustUinfoToPhaseOffset"
						    "\n");
					autoDigitalImpDetector->
					    adjustUinfoToPhaseOffset(
						(short)word_04);
					word_04 = 0;
				}
				edprintf("V90Phase3Demodulator: V92Jd "
					 "detected @ %d\r\n", word_2c);
				word_30 = 6;
				state = (Phase3DemodulatorState)8;
				jdV92->unPackJdPhaseReset();
				word_2c = 0;
			} else {
				jdV92->unPackJdReset();
			}
		}
		if (word_2c == word_14 + 38760.0f) {
			state = (Phase3DemodulatorState)0x17;
			word_2c = 0;
			word_30 = 0x15;
			edprintf("V90Phase3Demodulator: V92JdDemod "
				 "TimeOut\r\n");
		}
		break;

	case 8:					/* V92JdPhaseDemod */
		P3D_DEMOD_BIT();
		P3D_BUMP_FRAME();
		if ((unsigned char)jdV92->unPackJdPhaseData(bit) != 0) {
			if (word_2c % 6 == 0) {
				edprintf("V90Phase3Demodulator: V92JdPhase "
					 "detected @ %d\r\n", word_2c);
				word_30 = 7;
				state = (Phase3DemodulatorState)9;
				word_2c = 0;
				word_404 = 0;
			} else {
				jdV92->unPackJdPhaseReset();
			}
		}
		if (word_2c == 24804.0f) {
			state = (Phase3DemodulatorState)0x17;
			word_2c = 0;
			word_30 = 0x15;
			edprintf("V90Phase3Demodulator: JdPhaseDemod "
				 "TimeOut\r\n");
		}
		break;

	case 9:					/* V92JdDemod */
		P3D_DEMOD_BIT();
		P3D_BUMP_FRAME();
		if (bit != 0)
			word_404 = 0;
		else
			word_404++;
		if (word_404 > 0xb && word_2c % 72 == 12) {
			edprintf("V90Phase3Demodulator: JdNot detected "
				 "@ %d\r\n", word_2c);
			word_30 = 8;
			if (word_410 != 0) {
				state = (Phase3DemodulatorState)0xd;
				if (DSPLIB_DEBUG_ON())
					dsplibs_debug_printf(
					    "V90Phase3Demodulator: changing "
					    "state to DILDemodQCfirstStudy\n");
			} else {
				state = (Phase3DemodulatorState)
				    (V90PW(params)[P3D_P_PROBING_MODE] == 0
					? 0xa : 0x11);
			}
			word_2c = 0;
			phase3Modulator.reset(pcmType, ucode,
					      (Phase3ModulatorState)9, 0,
					      0, 0, dil, 0);
			break;
		}
		if (word_2c == word_14 + 38760.0f) {
			state = (Phase3DemodulatorState)0x17;
			word_2c = 0;
			word_30 = 0x15;
			edprintf("V90Phase3Demodulator: JdDemod TimeOut\r\n");
		}
		break;

	case 10:				/* DILDemodFirstStudy */
		mod = &phase3Modulator;
		decision = (short)level;
		sym = (short)mod->generateSymbol();
		adid = autoDigitalImpDetector;
		if (mod->usingSegmentLevel != 0) {
			if ((short)adid->isAltRbs((short)word_04, ucode,
						  sample) != 0) {
				code = P3D_CODE(P3D_ABS(sym));
				decision = (short)(P3D_SIGN(level) *
				    P3D_LINMAPPALT(adid, code));
			} else {
				code = P3D_CODE(P3D_ABS(sym));
				decision = (short)(P3D_SIGN(level) *
				    P3D_LINMAPP(adid, code));
			}
		}
		word_408 = 1;
		adid->calculateLinearMeanAndVar((short)level, (short)sym,
						word_04);
		P3D_BUMP_FRAME();
		if (word_2c == (unsigned int)V90PW(params)[P3D_P_30C]) {
			adid->porcessFirstStudy();
			if ((short)adid->isThereAnyAltRbsPhase() != 0)
				P3D_COPY_440_TO_438();
			word_2c = 0;
			state = (Phase3DemodulatorState)0xb;
		} else if (word_2c == 0x9c40) {
			state = (Phase3DemodulatorState)0x18;
			word_2c = 0;
			word_30 = 0x15;
			edprintf("V90Phase3Demodulator: DILDemodFirstStudy "
				 "TimeOut\r\n");
		}
		P3D_CHECK_TERMINATED();
		break;

	case 11:				/* DILDemodSecondStudy */
		mod = &phase3Modulator;
		sym = (short)mod->generateSymbol();
		adid = autoDigitalImpDetector;
		adid->calculateLinearMeanAndVar((short)level, (short)sym,
						word_04);
		if (adid->short_2800[word_04] != 0) {
			if (P3D_ABS(sym) == ucodeLevel) {
				if ((short)adid->isAltRbs((short)word_04,
							  ucode, sample) != 0) {
					code = P3D_CODE(P3D_ABS(sym));
					decision = (short)(P3D_SIGN(level) *
					    P3D_LINMAPPALT(adid, code));
				} else {
					code = P3D_CODE(P3D_ABS(sym));
					decision = (short)(P3D_SIGN(level) *
					    P3D_LINMAPP(adid, code));
				}
			} else {
				decision = (short)level;
				code = P3D_CODE(P3D_ABS(sym));
				adid->addReceivedSampleToStorage(
				    (short)word_04, (unsigned char)code,
				    sample);
			}
		} else {
			if (P3D_ABS(sym) != ucodeLevel) {
				code = P3D_CODE(P3D_ABS(sym));
				adid->updateLinMappMeanAndVar((short)word_04,
							      (short)code);
			}
			code = P3D_CODE(P3D_ABS(sym));
			decision = (short)(P3D_SIGN(level) *
			    P3D_LINMAPP(adid, code));
		}
		word_408 = 1;
		if (mod->segmentPos == 0 && mod->dilPcmCode != ucode)
			adid->uniteLinMappInfoOfUnsuspectedPhases(
			    mod->dilPcmCode);
		if (word_2c == (unsigned int)V90PW(params)[P3D_P_314]) {
			word_30 = 0xc;
		} else if (word_2c == (unsigned int)
			   V90PW(params)[P3D_P_300]) {
			if (short_400 != 0) {
				word_30 = 0x10;
				V90PW(params)[P3D_P_31C] =
				    V90PW(params)[P3D_P_320];
			} else {
				word_30 = 0xa;
			}
		}
		P3D_BUMP_FRAME();
		if (word_2c == (unsigned int)V90PW(params)[P3D_P_308]) {
			if (V90PW(params)[P3D_P_310] == 0) {
				adid->porcessSecondStudy();
				word_30 = 0x11;
				state = (Phase3DemodulatorState)0x10;
			} else {
				word_30 = 0x10;
				state = (Phase3DemodulatorState)0xc;
			}
			word_2c = 0;
		} else if (word_2c == 0x9c40) {
			state = (Phase3DemodulatorState)0x18;
			word_2c = 0;
			word_30 = 0x15;
			edprintf("V90Phase3Demodulator: DILDemodSecondStudy "
				 "TimeOut\r\n");
		}
		P3D_CHECK_TERMINATED();
		break;

	case 12:				/* DILDemodThirdStudyStage */
		mod = &phase3Modulator;
		sym = (short)mod->generateSymbol();
		adid = autoDigitalImpDetector;
		if (adid->short_2800[word_04] != 0) {
			decision = (short)level;
			if (mod->usingSegmentLevel == 0) {
				code = P3D_CODE(P3D_ABS(sym));
				adid->addReceivedSampleToStorage(
				    (short)word_04, (unsigned char)code,
				    sample);
			}
		} else {
			if (mod->usingSegmentLevel == 0) {
				adid->calculateLinearMeanAndVar((short)level,
				    (short)sym, word_04);
				code = P3D_CODE(P3D_ABS(sym));
				adid->updateLinMappMeanAndVar((short)word_04,
							      (short)code);
			}
			code = P3D_CODE(P3D_ABS(sym));
			decision = (short)(P3D_SIGN(level) *
			    P3D_LINMAPP(adid, code));
		}
		word_408 = 0;
		P3D_BUMP_FRAME();
		if (mod->segmentPos == 0 && mod->dilPcmCode != ucode)
			adid->uniteLinMappInfoOfUnsuspectedPhases(
			    mod->dilPcmCode);
		if (word_2c == (unsigned int)V90PW(params)[P3D_P_310]) {
			adid->porcessSecondStudy();
			word_2c = 0;
			word_30 = 0x11;
			state = (Phase3DemodulatorState)0x10;
		} else if (word_2c == 0x9c40) {
			state = (Phase3DemodulatorState)0x18;
			word_2c = 0;
			word_30 = 0x15;
			edprintf("V90Phase3Demodulator: "
				 "DILDemodThirdStudyStage TimeOut\r\n");
		}
		P3D_CHECK_TERMINATED();
		break;

	case 13:				/* DILDemodQCfirstStudy */
		mod = &phase3Modulator;
		decision = (short)level;
		sym = (short)mod->generateSymbol();
		adid = autoDigitalImpDetector;
		if (mod->usingSegmentLevel != 0) {
			if ((short)adid->isAltRbs((short)word_04, ucode,
						  sample) != 0) {
				code = P3D_CODE(P3D_ABS(sym));
				decision = (short)(P3D_SIGN(level) *
				    P3D_LINMAPPALT(adid, code));
			} else {
				code = P3D_CODE(P3D_ABS(sym));
				decision = (short)(P3D_SIGN(level) *
				    P3D_LINMAPP(adid, code));
			}
		}
		word_408 = 1;
		adid->calculateLinearMeanAndVar((short)level, (short)sym,
						word_04);
		P3D_BUMP_FRAME();
		if (word_2c == (unsigned int)V90PW(params)[P3D_P_30C]) {
			adid->porcessFirstStudy();
			if ((short)adid->isThereAnyAltRbsPhase() != 0)
				P3D_COPY_440_TO_438();
			word_2c = 0;
			state = (Phase3DemodulatorState)0xe;
		} else if (word_2c == 0x9c40) {
			state = (Phase3DemodulatorState)0x18;
			word_2c = 0;
			word_30 = 0x15;
			edprintf("V90Phase3Demodulator: DILDemodQCfirstStudy "
				 "TimeOut\r\n");
		}
		P3D_CHECK_TERMINATED();
		break;

	case 14:				/* DILDemodQCsecondStudy */
		mod = &phase3Modulator;
		sym = (short)mod->generateSymbol();
		adid = autoDigitalImpDetector;
		adid->calculateLinearMeanAndVar((short)level, (short)sym,
						word_04);
		if (adid->short_2800[word_04] != 0) {
			if (P3D_ABS(sym) == ucodeLevel) {
				if ((short)adid->isAltRbs((short)word_04,
							  ucode, sample) != 0) {
					code = P3D_CODE(P3D_ABS(sym));
					decision = (short)(P3D_SIGN(level) *
					    P3D_LINMAPPALT(adid, code));
				} else {
					code = P3D_CODE(P3D_ABS(sym));
					decision = (short)(P3D_SIGN(level) *
					    P3D_LINMAPP(adid, code));
				}
			} else {
				decision = (short)level;
				code = P3D_CODE(P3D_ABS(sym));
				adid->addReceivedSampleToStorage(
				    (short)word_04, (unsigned char)code,
				    sample);
			}
		} else {
			if (P3D_ABS(sym) == ucodeLevel) {
				code = P3D_CODE(P3D_ABS(sym));
				decision = (short)(P3D_SIGN(level) *
				    P3D_LINMAPP(adid, code));
			} else {
				decision = (short)level;
				if (adid->byte_280c[word_04] == 0) {
					code = P3D_CODE(P3D_ABS(sym));
					decision = (short)(P3D_SIGN(level) *
					    P3D_PREVLINMAPP(adid, code));
				}
			}
		}
		word_408 = 1;
		if (word_2c == (unsigned int)V90PW(params)[P3D_P_314]) {
			word_30 = 0xc;
		} else if (word_2c == (unsigned int)
			   V90PW(params)[P3D_P_300]) {
			if (short_400 != 0) {
				word_30 = 0x10;
				V90PW(params)[P3D_P_31C] =
				    V90PW(params)[P3D_P_320];
			} else {
				word_30 = 0xa;
			}
		}
		P3D_BUMP_FRAME();
		if (word_2c == (unsigned int)V90PW(params)[P3D_P_308]) {
			word_2c = 0;
			word_30 = 0x10;
			state = (Phase3DemodulatorState)0xf;
		} else if (word_2c == 0x9c40) {
			state = (Phase3DemodulatorState)0x18;
			word_2c = 0;
			word_30 = 0x15;
			edprintf("V90Phase3Demodulator: DILDemodQCsecondStudy "
				 "TimeOut\r\n");
		}
		P3D_CHECK_TERMINATED();
		break;

	case 15:				/* DILDemodQCthirdStudy */
		mod = &phase3Modulator;
		sym = (short)mod->generateSymbol();
		adid = autoDigitalImpDetector;
		if (mod->usingSegmentLevel == 0)
			adid->calculateLinearMeanAndVar((short)level,
			    (short)sym, word_04);
		if (adid->short_2800[word_04] != 0) {
			decision = (short)level;
			if (mod->usingSegmentLevel == 0) {
				code = P3D_CODE(P3D_ABS(sym));
				adid->addReceivedSampleToStorage(
				    (short)word_04, (unsigned char)code,
				    sample);
			}
		} else {
			decision = (short)level;
			if (adid->byte_280c[word_04] == 0) {
				code = P3D_CODE(P3D_ABS(sym));
				decision = (short)(P3D_SIGN(level) *
				    P3D_PREVLINMAPP(adid, code));
			}
		}
		word_408 = 0;
		P3D_BUMP_FRAME();
		if (word_2c == (unsigned int)V90PW(params)[P3D_P_310]) {
			adid->setQcLinearMapping();
			word_2c = 0;
			word_30 = 0x11;
			state = (Phase3DemodulatorState)0x10;
		} else if (word_2c == 0x9c40) {
			state = (Phase3DemodulatorState)0x18;
			word_2c = 0;
			word_30 = 0x15;
			edprintf("V90Phase3Demodulator: DILDemodQCthirdStudy "
				 "TimeOut\r\n");
		}
		P3D_CHECK_TERMINATED();
		break;

	case 16:				/* DILDemodErrorRelaxation */
		mod = &phase3Modulator;
		sym = (short)mod->generateSymbol();
		adid = autoDigitalImpDetector;
		if (adid->short_2800[word_04] != 0) {
			decision = (short)level;
			if (mod->usingSegmentLevel != 0) {
				if ((short)adid->isAltRbs((short)word_04,
							  ucode, sample) != 0) {
					code = P3D_CODE(P3D_ABS(sym));
					decision = (short)(P3D_SIGN(level) *
					    P3D_LINMAPPALT(adid, code));
				} else {
					code = P3D_CODE(P3D_ABS(sym));
					decision = (short)(P3D_SIGN(level) *
					    P3D_LINMAPP(adid, code));
				}
			}
		} else {
			code = P3D_CODE(P3D_ABS(sym));
			decision = (short)(P3D_SIGN(level) *
			    P3D_LINMAPP(adid, code));
		}
		P3D_BUMP_FRAME();
		if (byte_3f9 != 0) {
			word_3fc++;
			if (word_3fc == (unsigned int)
			    V90PW(params)[P3D_P_31C]) {
				word_30 = 0xf;
				word_408 = 1;
			}
		}
		if (word_2c == (unsigned int)V90PW(params)[P3D_P_318]) {
			word_30 = 0x12;
		} else if (word_2c == 0x9c40) {
			state = (Phase3DemodulatorState)0x18;
			word_2c = 0;
			word_30 = 0x15;
			edprintf("V90Phase3Demodulator: "
				 "DILDemodErrorRelaxation TimeOut\r\n");
		}
		P3D_CHECK_TERMINATED();
		break;

	case 17:				/* ProbingDILDemod */
		mod = &phase3Modulator;
		decision = (short)level;
		sym = (short)mod->generateSymbol();
		if (P3D_ABS(sym) == 0xffc &&
		    mod->segmentPos > (unsigned int)
			V90PW(params)[P3D_P_DFE_LENGTH])
			word_408 = 1;
		else
			word_408 = 0;
		if (word_2c == dilLength) {
			edprintf("V90Phase3Demodulator: Probing DIL "
				 "ended\r\n");
			word_30 = 0x13;
		} else if (word_2c == 0x9c40) {
			state = (Phase3DemodulatorState)0x18;
			word_2c = 0;
			word_30 = 0x15;
			edprintf("V90Phase3Demodulator: ProbingDILDemod "
				 "TimeOut\r\n");
		}
		break;

	case 19:
	case 20:
	case 21:
	case 22:
	case 23:
	case 24:
	case 25:
		decision = 0;
		break;

	case 26:				/* WaitForQts */
		decision = (short)level;
		ansamToneDetector->process(sample);
		if (sdDetector->process(sample) > 0) {
			edprintf("V90Phase3Demodulator: QTS detected "
				 "@ %d\r\n", word_2c);
			state = (Phase3DemodulatorState)0x1b;
			word_2c = 0;
			word_30 = 0x37;
		} else if (word_2c ==
			   (unsigned int)V90PW(params)[P3D_P_ANSPCM_LENGTH]) {
			state = (Phase3DemodulatorState)0x20;
			word_2c = 0;
			verificationStatus = 0;
			word_30 = 0x3a;
			edprintf("V90Phase3Demodulator: WaitFor Qts TimeOut, "
				 "decision is set to not same line\r\n");
		}
		break;

	case 27:				/* WaitForQtsNot */
		decision = (short)level;
		ansamToneDetector->process(sample);
		if (sdDetector->process(sample) < 0) {
			edprintf("V90Phase3Demodulator: QTSNot detected "
				 "@ %d\r\n", word_2c);
			state = (Phase3DemodulatorState)0x1c;
			word_2c = 0;
			word_30 = 0x38;
		} else if (word_2c == 0x300) {
			state = (Phase3DemodulatorState)0x21;
			word_2c = 0;
			verificationStatus = 0;
			edprintf("V90Phase3Demodulator: WaitFor QtsNot "
				 "TimeOut, decision is set to not same "
				 "line\r\n");
			if (((*(const unsigned char *const *)(const void *)
			      params)[0] & 1) != 0)
				V90PW(params)[P3D_P_ANSPCM_LENGTH] = 0x320;
		}
		break;

	case 28:				/* enter ANSpcm demod */
		decision = (short)level;
		if (word_2c == 0x30) {
			state = (Phase3DemodulatorState)0x1d;
			word_2c = 0;
			if (((*(const unsigned char *const *)(const void *)
			      params)[0] & 1) != 0)
				V90PW(params)[P3D_P_ANSPCM_LENGTH] = 0x320;
			word_30 = 0x39;
			verificationStatus = 1;
			ansamToneDetector->reset();
			edprintf("V90Phase3Demodulator: enter ANSpcm demod "
				 "state\r\n");
		}
		break;

	case 29:				/* ANSpcm demod */
		decision = (short)level;
		ansamToneDetector->process(sample);
		if (word_2c ==
		    (unsigned int)V90PW(params)[P3D_P_ANSPCM_LENGTH])
			word_30 = 0x3a;
		break;

	case 30:				/* ANSpcm energy drop watch */
		decision = (short)level;
		if (ansamToneDetector->process(sample) == 0) {
			edprintf("V90Phase3Demodulator: ANSpcm energy drop "
				 "detected...\r\n");
			word_30 = 0x3b;
			state = (Phase3DemodulatorState)0x1f;
		}
		break;

	case 31:
		decision = (short)level;
		break;

	case 32:
		decision = (short)level;
		if (ansamToneDetector->process(sample) != 0) {
			edprintf("V90Phase3Demodulator: ANSpcm detected on "
				 "recovery, move to wait for drop...\r\n");
			word_30 = 0x3a;
		}
		if (word_2c > 0x7cf) {
			edprintf("V90Phase3Demodulator: faking ANSpcm energy "
				 "drop detected (QTs timeout)...\r\n");
			word_30 = 0x3b;
			state = (Phase3DemodulatorState)0x1f;
		}
		break;

	case 33:
		decision = (short)level;
		if (ansamToneDetector->process(sample) != 0 &&
		    word_2c >= (unsigned int)
			V90PW(params)[P3D_P_ANSPCM_LENGTH]) {
			edprintf("V90Phase3Demodulator: ANSpcm detected on "
				 "recovery, move to wait for drop...\r\n");
			word_30 = 0x3a;
		}
		break;
	}

	return decision;
}

#undef P3D_ABS
#undef P3D_SIGN
#undef P3D_CODE
#undef P3D_LINMAPP
#undef P3D_LINMAPPALT
#undef P3D_PREVLINMAPP
#undef P3D_COPY_440_TO_438
#undef P3D_BUMP_FRAME
#undef P3D_CHECK_TERMINATED
#undef P3D_DEMOD_BIT
#undef P3D_PHASE

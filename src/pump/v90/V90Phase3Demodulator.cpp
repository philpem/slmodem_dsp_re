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
#include "dsplib/pcm.h"
#include "dsplib/debug.h"
}

#include "dsplib/V90Dil.h"
#include "dsplib/V90Phase3Demodulator.h"

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

	word_3cc = 0;
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
	: phase3Modulator(p, flag), word_3cc(0),
	  descrambler(P3D_DSC_TAP1, P3D_DSC_TAIL, P3D_DSC_OUT)
{
	V90SdDetector *sd;
	ANSamToneDetector *an;

	(void)unused;

	params = p;
	sessionFlag = flag;
	autoDigitalImpDetector = adid;

	sd = (V90SdDetector *)sysdep_malloc(sizeof(V90SdDetector));
	v90p3d_sd_ctor(sd, params->f[PARAMS_SD_THRESH_08],
		       params->f[PARAMS_SD_THRESH_0C],
		       params->f[PARAMS_SD_VALUE_10],
		       (unsigned int)params->w[PARAMS_SD_LIMIT]);
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

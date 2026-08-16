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
P3D_OFF(word_3f4,		0x3f4, word3f4);
P3D_OFF(byte_3f9,		0x3f9, byte3f9);
P3D_OFF(word_3fc,		0x3fc, word3fc);
P3D_OFF(short_400,		0x400, short400);
P3D_OFF(word_404,		0x404, word404);
P3D_OFF(word_408,		0x408, word408);
P3D_OFF(dilLength,		0x40c, dillength);
P3D_OFF(word_420,		0x420, word420);
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

/* ============================================================ getV92Decision */

#include "dsplib/DiffCoder.h"
#include "dsplib/V92Jd.h"

extern "C" {
#include "dsplib/encode.h"
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
 * `Phase3DemodulatorState`, because `getV90Decision` is being written against
 * that enum concurrently and it must not move.  What the strings settle:
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

/* |x|, as `cltd; xor; sub` -- what GCC emits for this at -O2. */
#define P3D_ABS(x)		((x) < 0 ? -(x) : (x))

/* `sar $0x1f; or $0x1`: -1 for a negative level, +1 otherwise. */
#define P3D_SIGN(x)		(((x) >> 31) | 1)

/* The A-law or mu-law index of a linear magnitude. */
#define P3D_CODE(v) \
	(pcmType == PCM_TYPE_MU_LAW \
	    ? (int)(unsigned char)~linear2ulaw(v) \
	    : (int)(unsigned char)(linear2alaw(v) ^ 0xd5))

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
 * mov %eax,0x438(%c)` -- a raw 32-bit copy between a slot this tree types
 * `float` and one it types `int`, so it is spelled as the word view.
 */
#define P3D_COPY_440_TO_438() \
	(V90PW(params)[P3D_P_438] = V90PW(params)[P3D_P_440])

/* +0x3cc is a SerialDifferentialDecoder<int>; see the header. */
#define P3D_SERIAL() \
	((SerialDifferentialDecoder<int> *)(void *)&word_3cc)

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
		bit = P3D_SERIAL()->process(bit); \
		bit = descrambler.process(bit); \
		decision = (short)level; \
	} while (0)

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
#undef P3D_SERIAL
#undef P3D_BUMP_FRAME
#undef P3D_CHECK_TERMINATED
#undef P3D_DEMOD_BIT

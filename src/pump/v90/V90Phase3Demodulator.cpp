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

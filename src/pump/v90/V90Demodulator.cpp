/*
 * V90Demodulator::enterPhase3 -- start the receiver's phase 3, from dsplibs.o.
 *
 * A LATCH, A FAN-OUT AND TWO EXITS.  The method does nothing at all if it has
 * already run; otherwise it prints, latches, clears four words, resets six
 * subobjects through four different kinds of reference -- embedded, pointed
 * at, reached through the parameter block, and reached through a pointer
 * INSIDE the parameter block -- and then chooses between two ways of
 * finishing.
 *
 * `V90PreFilter::isV90WithEia6()` IS CALLED TWICE, and the second call is
 * kept because the blob makes it -- NOT because the answer can change.  It
 * cannot: `isV90WithEia6` is `(cap == 1) || (params->w[0x500/4] == 6)`, and
 * between the two calls the object runs `setParamEia6`, which writes thirty
 * words of the parameter block and none of them is +0x500, and
 * `displayParamEia6`, which is one `ret`.  Neither touches `refLoop` or
 * `codecType`, which is where `cap` comes from.  So caching the first result
 * would be equivalent, the mutation that does it is uncaught, and the suite
 * says so with the reason rather than pretending otherwise.  Finding 293.
 *
 * THE SECOND RESULT IS NARROWED TO `short` BEFORE IT BECOMES AN ARGUMENT.
 * `cwtl` in the blob, `(short)` here, and it looks redundant only until
 * `isV90WithEia6` returns something with a non-zero high half-word.
 *
 * THE TWO EXITS.  A signed byte at +0x02 of whatever the parameter block's
 * first word points at ends the method early when it is negative; otherwise a
 * non-zero `sessionFlag` -- the V.92 session -- takes the retrain exit, which
 * is the one place in wave 2 that writes a value other than 0 or 1.
 */

#include <stddef.h>

extern "C" {
#include "dsplib/debug.h"
#include "dsplib/encode.h"
}

/*
 * The header forward-declares this class, which is enough for the pointer
 * member; `reset` CALLS a member of it, so this file needs the definition.
 * V90ConstellationDesigner.h forward-declares `V90Parameters` and includes
 * nothing, so it cannot collide with the definition V90PreFilter.h supplies
 * below (finding 1112).
 */
#include "dsplib/V90ConstellationDesigner.h"
#include "dsplib/V90Demodulator.h"

#if defined(__SIZEOF_POINTER__) && __SIZEOF_POINTER__ == 4

#define DEM_OFF(field, off, tag) \
	typedef char v90dem_off_##tag[ \
	    ((int)__builtin_offsetof(V90Demodulator, field) == (off)) \
	    ? 1 : -1]

DEM_OFF(phase2Info,		0x004, phase2info);
DEM_OFF(jd,			0x008, jd);
DEM_OFF(jdV92,			0x00c, jdv92);
DEM_OFF(dil,			0x010, dil);
DEM_OFF(mappingParams,		0x014, mapping);
DEM_OFF(mappingParamsAlt,	0x018, mappingalt);
DEM_OFF(trn2Designer,		0x01c, trn2);
DEM_OFF(cp,			0x024, cp);
DEM_OFF(mp,			0x028, mp);
DEM_OFF(params,			0x02c, params);
DEM_OFF(sessionFlag,		0x030, sessionflag);
DEM_OFF(inPhase3,		0x034, inphase3);
DEM_OFF(word_38,		0x038, word38);
DEM_OFF(word_3c,		0x03c, word3c);
DEM_OFF(word_40,		0x040, word40);
DEM_OFF(word_44,		0x044, word44);
DEM_OFF(agc,			0x04c, agc);
DEM_OFF(preFilter,		0x06c, prefilter);
DEM_OFF(resampler,		0x094, resampler);
DEM_OFF(equalizer,		0x1d8, equalizer);
DEM_OFF(phase3Demodulator,	0x1dc, phase3demodulator);
DEM_OFF(phase4Demodulator,	0x1e0, phase4demodulator);
DEM_OFF(demapper,		0x1e4, demapper);
DEM_OFF(descrambler,		0x1e8, descrambler);
DEM_OFF(constellationDesigner,	0x208, constellationdesigner);
DEM_OFF(connectionEvaluator,	0x20c, connectionevaluator);
DEM_OFF(spectralVerifier,	0x210, spectralverifier);
DEM_OFF(autoDigitalImpDetector,	0x23c, adid);
DEM_OFF(word_24c,		0x24c, word24c);
DEM_OFF(word_258,		0x258, word258);
DEM_OFF(word_260,		0x260, word260);
DEM_OFF(word_264,		0x264, word264);
DEM_OFF(word_270,		0x270, word270);
DEM_OFF(word_284,		0x284, word284);
DEM_OFF(word_278,		0x278, word278);
DEM_OFF(byte_280,		0x280, byte280);
DEM_OFF(word_288,		0x288, word288);
DEM_OFF(word_28c,		0x28c, word28c);
DEM_OFF(word_290,		0x290, word290);
DEM_OFF(word_294,		0x294, word294);

/* Settled by the allocation that precedes the constructor; finding 291. */
typedef char v90dem_size[(sizeof(V90Demodulator) == 0x298) ? 1 : -1];

/*
 * `V90ConnectionEvaluator`'s size and its four offset assertions used to be
 * here, because the class was defined in this file's header.  It has its own
 * header and its own .cpp now (task #88) and they moved with it, unchanged.
 */

#endif

/*
 * Where the two words copied out of the parameter block live, and where the
 * timing offset comes from.  Spelled as indices because V90Parameters is a
 * block of unrecovered layout (V90PreFilter.h), and naming a field of it here
 * would be inventing one.
 */
#define PARAMS_TIMING_OFFSET	(0x084 / 4)
#define PARAMS_WORD_264		(0x264 / 4)
#define PARAMS_WORD_278		(0x278 / 4)

/*
 * The three `reset` adds.  The names in the comments are the ORIGINAL
 * AUTHOR'S, out of `include/dsplib/V90Parameters.h` -- that header cannot be
 * included here, because this file already has the other definition of the
 * class (finding 1112), but the map it carries is still what these indices
 * mean and writing them down without it would be throwing information away.
 */
#define PARAMS_AGC_NOMINAL_ENERGY	(0x05c / 4)	/* float */
#define PARAMS_AGC_BLOCK_LEN		(0x064 / 4)	/* int   */
#define PARAMS_LINEAR_EQU_CURSOR_PLACE	(0x184 / 4)	/* int   */

/*
 * `V90Resampler::reset()` BY ITS MANGLED NAME, and it is not a shortcut.
 *
 * The object at +0x94 is a V90Resampler -- the constructor builds one there
 * and finding 804 finds `V90Resampler`'s vtable pointer at that offset -- and
 * `reset` calls `_ZN12V90Resampler5resetEv` on it DIRECTLY, not through the
 * vptr.  Three things rule out the obvious spellings:
 *
 *   - `resampler.reset()` would dispatch through the vptr, which no test
 *     fixture here fills in, and would be an indirect call where the object
 *     makes a direct one.
 *   - `((V90Resampler *)&resampler)->V90Resampler::reset()` needs the class
 *     to be complete, and `V90Resampler.h` includes the OTHER definition of
 *     `V90Parameters`, which this file cannot have.
 *   - Declaring the member type as `V90Resampler` in the header has the same
 *     problem one level up, since `V90Demodulator.h` includes
 *     `V90PreFilter.h`.
 *
 * So the symbol is named directly.  It takes `this` as its first stack
 * argument like every other member here (finding 215), and
 * `ResamplerTimingOffset` is `V90Resampler`'s base at offset zero, so the
 * address is the same one `setTimingOffset` is already called on.
 */
extern void v90resampler_reset(ResamplerTimingOffset *self)
	asm("_ZN12V90Resampler5resetEv");

void
V90Demodulator::enterPhase3()
{
	const signed char *block;

	/*
	 * EXACTLY 1, not merely non-zero.  `cmpl $0x1,0x34(%esi)`, and the
	 * distinction is real: any other non-zero value runs the whole method.
	 */
	if (inPhase3 == 1)
		return;

	if (DSPLIB_DEBUG_ON())
		dsplibs_debug_printf("V90Demodulator: enter Phase 3\r\n");

	inPhase3 = 1;
	word_44 = word_38;
	word_38 = 0;
	word_3c = 0;
	word_40 = 0;

	phase2Info->printInfo();
	preFilter.selectFilter();

	if (preFilter.isV90WithEia6()) {
		preFilter.setParamEia6();
		preFilter.displayParamEia6();
		/*
		 * The resampler is EMBEDDED at +0x94 -- `lea 0x94(%esi),%ecx`
		 * -- and the offset is read out of the parameter block, not
		 * out of this object.
		 */
		resampler.setTimingOffset(params->f[PARAMS_TIMING_OFFSET]);
	}

	spectralVerifier.reset();

	/*
	 * The second call; see the file comment on why it is not the first
	 * one's result.  The `short` is the blob's `cwtl`.
	 */
	phase3Demodulator->reset((PcmType)phase2Info->pcmType,
				 phase2Info->Uinfo,
				 (Phase3DemodulatorState)0, 0,
				 jd, jdV92, dil,
				 (short)preFilter.isV90WithEia6(), 1, 0.0f,
				 (unsigned int)phase2Info->rtd);

	/*
	 * AFTER the reset, which has just zeroed this very field.  The order
	 * is the whole content of the store.
	 */
	phase3Demodulator->word_410 = word_294;

	equalizer->enterPhase3();

	word_288 = params->w[PARAMS_WORD_264];
	word_290 = params->w[PARAMS_WORD_278];

	/*
	 * Four words of the evaluator, all zero.  The blob writes them
	 * +0x84, +0x70, +0x88, +0x74; they are in offset order here because
	 * four stores of the same constant to four distinct members cannot be
	 * told apart by their order.
	 */
	connectionEvaluator->word_70 = 0;
	connectionEvaluator->word_74 = 0;
	connectionEvaluator->word_84 = 0;
	connectionEvaluator->word_88 = 0;

	byte_280 = 0;

	/*
	 * `mov (%edx),%ebx; cmpb $0x0,0x2(%ebx); js` -- a pointer out of the
	 * parameter block's first word, and a SIGNED byte two in from it.  The
	 * class it points at is not modelled and this is the only thing in
	 * wave 2 that reaches it.
	 */
	block = *(const signed char *const *)&params->b[0];
	if (block[2] < 0)
		return;

	if (sessionFlag != 0) {
		edprintf("V90Demodulator: we got PCM upstream under V.92Lite, "
			 "retraining to V.34 upstream...\r\n");
		word_3c = 0x20;
	}
}

/*
 * reInit -- two calls and nothing else.  41 bytes, the second a tail call.
 *
 * `VPcmFloModem::externalReset` is the only caller and reaches it only when
 * its own +0x6120 is non-zero.
 */
void
V90Demodulator::reInit()
{
	connectionEvaluator->reset();
	phase3Demodulator->clearVerificationStatus();
}

/*
 * reset -- the demodulator and five of the objects it owns.
 *
 * FOUR THINGS IN IT ARE WORTH WRITING DOWN.
 *
 * 1. IT RECONFIGURES THE AGC IMMEDIATELY AFTER RESETTING IT, through the
 *    pointer it already had in a register: `Agc<float>::reset` clears the
 *    block state, and then two words come out of the parameter block into
 *    `blockLen` and `ref`.  Resetting an AGC does NOT restore its
 *    configuration in this object -- the caller does.
 *
 * 2. THE BAUD-OFFSET DIAGNOSTIC IS UNGATED, and it is the only floating-point
 *    arithmetic in the function.  It splits `INITIAL_BAUD_OFFSET` into a sign
 *    character, a truncated magnitude and three decimal places, the same
 *    `%c%d.%03d` shape `V90PreFilter::setParamEia6` uses at four places.  The
 *    sign test is `0.0f < x`, taken from `fcomps` with zero on the stack and
 *    the parameter as the operand, so a NaN offset prints '-'.
 *
 * 3. THE EQUALISER'S CURSOR IS EITHER CONFIGURED OR DERIVED.  A negative
 *    `LINEAR_EQU_CURSOR_PLACE` means "the middle of the linear equaliser",
 *    computed as `linearEquLength >> 1` -- a LOGICAL shift, which is the
 *    second independent statement that the length is unsigned.  The test is
 *    `js`, so zero takes the configured branch.
 *
 * 4. THE LAST STORE IS INTO ANOTHER OBJECT.  `quickConnect` goes to this
 *    object's +0x294 and then to the EQUALISER's +0x148, which is the only
 *    write anywhere in the blob to that offset of that class and the reason
 *    `sizeof(V90Equalizer)` is 0x150 (finding 1107).
 */
void
V90Demodulator::reset(unsigned int quickConnect)
{
	float offset;
	int whole, frac, cursor;

	if (DSPLIB_DEBUG_ON())
		dsplibs_debug_printf(
		    "V90Demodulator reset, quick connect flag = %d\r\n",
		    quickConnect);

	inPhase3 = 0;
	word_27c = 0;
	word_270 = 0;
	word_38 = 0;
	word_3c = 0;
	word_44 = 0;
	word_40 = 0;

	descrambler.reset(0);

	agc.reset();
	agc.blockLen = params->w[PARAMS_AGC_BLOCK_LEN];
	agc.ref = params->f[PARAMS_AGC_NOMINAL_ENERGY];

	preFilter.reset();

	offset = params->f[PARAMS_TIMING_OFFSET];
	whole = (int)offset;
	frac = (int)((offset - (float)whole) * 1000.0f);
	edprintf("V90Demodulator reset: Baud Offset = %c%d.%03d\r\n",
		 (0.0f < offset) ? '+' : '-',
		 (int)__builtin_fabsf(offset),
		 (frac < 0) ? -frac : frac);

	v90resampler_reset(&resampler);
	resampler.setTimingOffset(params->f[PARAMS_TIMING_OFFSET]);

	cursor = params->w[PARAMS_LINEAR_EQU_CURSOR_PLACE];
	if (cursor < 0)
		equalizer->reset(equalizer->linearEquLength >> 1);
	else
		equalizer->reset((unsigned int)cursor);

	constellationDesigner->reset();

	word_290 = 19200;
	word_284 = 0;
	word_24c = 0;
	word_288 = 19200;
	word_28c = 0;
	word_258 = 0;
	word_260 = 0;
	word_278 = 0;
	byte_280 = 0;
	word_294 = quickConnect;

	equalizer->quickConnect = quickConnect;
}

/*
 * enterChannelVerification -- `reset(1)` and then the phase 3 chain.
 *
 * THE FIRST ARGUMENT IS NEVER READ.  `0x44(%esp)` is not loaded anywhere in
 * the 215 bytes; only the second reaches anything, as `V90Phase3Demodulator::
 * reset`'s ninth argument, sign-extended with `movswl` at the top of the
 * function.  It is left unnamed rather than removed, because the mangling
 * `_ZN14V90Demodulator24enterChannelVerificationEss` has two `s` in it and
 * dropping one would emit a symbol that links against nothing.
 *
 * The phase 3 reset is the same eleven-argument call `enterPhase3` makes,
 * with three of the arguments different: the state is WaitForQTS rather than
 * WaitForSd, `altRbs` is 0 rather than the EIA-6 answer, and `short414` is
 * the caller's second argument rather than 1.
 */
void
V90Demodulator::enterChannelVerification(short, short short414)
{
	if (DSPLIB_DEBUG_ON())
		dsplibs_debug_printf(
		    "V90Demodulator: Enter Channel Verification called !\r\n");

	reset(1);

	inPhase3 = 5;
	word_38 = 0;

	phase3Demodulator->clearVerificationStatus();

	phase3Demodulator->reset((PcmType)phase2Info->pcmType,
				 phase2Info->Uinfo,
				 P3D_STATE_WAIT_FOR_QTS, 0,
				 jd, jdV92, dil,
				 0, short414, 0.0f,
				 (unsigned int)phase2Info->rtd);

	equalizer->enterChannelVerification();

	word_40 = 0;
}

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
DEM_OFF(word_264,		0x264, word264);
DEM_OFF(word_278,		0x278, word278);
DEM_OFF(byte_280,		0x280, byte280);
DEM_OFF(word_288,		0x288, word288);
DEM_OFF(word_290,		0x290, word290);
DEM_OFF(word_294,		0x294, word294);

/* Both settled by the allocation that precedes the constructor; finding 291. */
typedef char v90dem_size[(sizeof(V90Demodulator) == 0x298) ? 1 : -1];
typedef char v90ce_size[(sizeof(V90ConnectionEvaluator) == 0xbc) ? 1 : -1];

#define CE_OFF(field, off, tag) \
	typedef char v90ce_off_##tag[ \
	    ((int)__builtin_offsetof(V90ConnectionEvaluator, field) == (off)) \
	    ? 1 : -1]

CE_OFF(word_70, 0x70, word70);
CE_OFF(word_74, 0x74, word74);
CE_OFF(word_84, 0x84, word84);
CE_OFF(word_88, 0x88, word88);

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

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
 * cannot: `isV90WithEia6` is `(cap == 1) || (V90PW(params)[0x500/4] == 6)`, and
 * between the two calls the object runs `setParamEia6`, which writes thirty
 * words of the parameter block and none of them is +0x500, and
 * `displayParamEia6`, which is one `ret`.  Neither touches `refLoop` or
 * `codecType`, which is where `cap` comes from.  So caching the first result
 * would be equivalent, the mutation that does it is uncaught, and the suite
 * says so with the reason rather than pretending otherwise.  Finding F293.
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
 * below (finding F1112).
 */
#include "dsplib/V90ConstellationDesigner.h"
#include "dsplib/V90Demodulator.h"
/*
 * The four classes the constructor allocates whose destructors the destructor
 * calls explicitly, and which the header only forward-declares.  Each of the
 * four headers either includes nothing or includes only classes that forward-
 * declare `V90Parameters`, so none of them collides with the block definition
 * V90PreFilter.h supplies through the header above (finding F1112).  The other
 * four -- V90Equalizer, V90Phase3Demodulator, V90AutoDigitalImpDetector and
 * V90ConnectionEvaluator -- arrive with V90Demodulator.h already.
 */
#include "dsplib/V90Demapper.h"
#include "dsplib/V90Phase4Demodulator.h"
#include "dsplib/V90TRN2Designer.h"
#include "dsplib/sysdep.h"
/*
 * `getAT_UD`'s argument record, and the block `enterRRN` writes +0x10 of.
 * Both headers define one struct and include nothing, so neither can collide
 * with the block definition of `V90Parameters` that V90PreFilter.h supplies
 * through V90Demodulator.h (finding F1112).
 */
#include "dsplib/TAG_DiagnosticResults.h"
#include "dsplib/tagV90AdditionalCPinfo.h"
/*
 * The header forward-declares this one too, and `getBitRate` DEREFERENCES it.
 * Same argument as V90ConstellationDesigner above.
 */
#include "dsplib/V90MappingParams.h"

/*
 * And this one, for the same reason again: the header forward-declares
 * `V90MP` because it only ever holds a pointer, and `progress` reads six of
 * its fields for the "'Problematic' ISP Modem" signature test.
 */
#include "dsplib/V90MP.h"

#if defined(__SIZEOF_POINTER__) && __SIZEOF_POINTER__ == 4

#define DEM_OFF(field, off, tag) \
	typedef char v90dem_off_##tag[ \
	    ((int)__builtin_offsetof(V90Demodulator, field) == (off)) \
	    ? 1 : -1]

DEM_OFF(codecType,		0x000, codectype);
DEM_OFF(phase2Info,		0x004, phase2info);
DEM_OFF(jd,			0x008, jd);
DEM_OFF(jdV92,			0x00c, jdv92);
DEM_OFF(dil,			0x010, dil);
DEM_OFF(mappingParams,		0x014, mapping);
DEM_OFF(mappingParamsAlt,	0x018, mappingalt);
DEM_OFF(trn2Designer,		0x01c, trn2);
DEM_OFF(additionalCPinfo,	0x020, acpinfo);
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
DEM_OFF(constellationPower,	0x148, cpower);
DEM_OFF(equalizer,		0x1d8, equalizer);
DEM_OFF(phase3Demodulator,	0x1dc, phase3demodulator);
DEM_OFF(phase4Demodulator,	0x1e0, phase4demodulator);
DEM_OFF(demapper,		0x1e4, demapper);
DEM_OFF(descrambler,		0x1e8, descrambler);
DEM_OFF(constellationDesigner,	0x208, constellationdesigner);
DEM_OFF(connectionEvaluator,	0x20c, connectionevaluator);
DEM_OFF(spectralVerifier,	0x210, spectralverifier);
DEM_OFF(autoDigitalImpDetector,	0x23c, adid);
DEM_OFF(array_244,		0x244, array244);
DEM_OFF(array_248,		0x248, array248);
DEM_OFF(word_24c,		0x24c, word24c);
DEM_OFF(array_250,		0x250, array250);
DEM_OFF(array_254,		0x254, array254);
DEM_OFF(word_258,		0x258, word258);
DEM_OFF(array_25c,		0x25c, array25c);
DEM_OFF(word_260,		0x260, word260);
DEM_OFF(word_264,		0x264, word264);
DEM_OFF(word_270,		0x270, word270);
DEM_OFF(errorEnergyPrintCounter,		0x284, word284);
DEM_OFF(word_278,		0x278, word278);
DEM_OFF(byte_280,		0x280, byte280);
DEM_OFF(errorEnergyPrintPeriod,		0x288, word288);
DEM_OFF(timingOffsetPrintCounter,		0x28c, word28c);
DEM_OFF(timingOffsetPrintPeriod,		0x290, word290);
DEM_OFF(quickConnect,		0x294, quickconnect);

/*
 * `getBitRate` multiplies `mappingParamsAlt`'s FIRST word, and nothing in
 * this tree can mutate that placement -- it is in another file's header --
 * so the assert costs no empirical guarantee (tiers.md's rule about a static
 * check displacing a mutation).
 */
typedef char v90dem_mpar_word0[
    (__builtin_offsetof(V90MappingParams, word_0) == 0) ? 1 : -1];

/* Settled by the allocation that precedes the constructor; finding F291. */
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
 * The four the phase-4 and data-phase entries copy into +0x288 and +0x290.
 * `enterPhase3` already used +0x264 and +0x278 for the same PAIR of
 * destinations, so the parameter block holds one (+0x288, +0x290) pair per
 * phase and these are phases 4 and data:
 *
 *     enterPhase3           +0x264 -> +0x288      +0x278 -> +0x290
 *     enterRRN, enterFPE,
 *     enterPhase4           +0x268 -> +0x288      +0x27c -> +0x290
 *     enterDataPhase        +0x26c -> +0x288      +0x280 -> +0x290
 *
 * -- three consecutive words in each of two runs, indexed by phase.  That is
 * the shape of the block and not a name for either quantity, so these keep
 * offset names: the author's own names are in V90Parameters.h, which this
 * file cannot include (finding F1112), and inventing two here would be worse
 * than an offset.
 */
#define PARAMS_WORD_268		(0x268 / 4)
#define PARAMS_WORD_26C		(0x26c / 4)
#define PARAMS_WORD_27C		(0x27c / 4)
#define PARAMS_WORD_280		(0x280 / 4)

/*
 * `sessionTermination`'s two.  The names in the comments are the ORIGINAL
 * AUTHOR'S, out of `include/dsplib/V90Parameters.h`, for the reason the three
 * `reset` indices below give: this file cannot include that header, and
 * writing the offsets down without the names it carries throws information
 * away.  `TIMING_OFFESET` is the author's spelling.
 */
#define PARAMS_TIMING_HISTORY_EVAL	(0x160 / 4)	/* int   */
#define PARAMS_MIN_STD_FOR_SAVE		(0x16c / 4)	/* float */

/*
 * Where the timing offset is SAVED, which is not in the parameter block at
 * all: it is +0x4c of the `_tagModemParameters` the block's first word points
 * at, as a signed count of thousandths.  The same word is what
 * `V90PreFilter::setParamEia6` READS as "prev params ClockDeviation", by the
 * same two-step dereference and with the reciprocal scale (`* 0.001f`).
 * Finding F1274; the pairing is why "saved in Registry" is not a figure of
 * speech.
 */
#define MODEM_CLOCK_DEVIATION	(0x04c / 4)

/*
 * The three `reset` adds.  The names in the comments are the ORIGINAL
 * AUTHOR'S, out of `include/dsplib/V90Parameters.h` -- that header cannot be
 * included here, because this file already has the other definition of the
 * class (finding F1112), but the map it carries is still what these indices
 * mean and writing them down without it would be throwing information away.
 */
#define PARAMS_AGC_NOMINAL_ENERGY	(0x05c / 4)	/* float */
#define PARAMS_AGC_BLOCK_LEN		(0x064 / 4)	/* int   */
#define PARAMS_LINEAR_EQU_CURSOR_PLACE	(0x184 / 4)	/* int   */

/*
 * `V90Resampler::reset()` BY ITS MANGLED NAME, and it is not a shortcut.
 *
 * The object at +0x94 is a V90Resampler -- the constructor builds one there
 * and finding F804 finds `V90Resampler`'s vtable pointer at that offset -- and
 * `reset` calls `_ZN12V90Resampler5resetEv` on it DIRECTLY, not through the
 * vptr.  ONE thing rules out the obvious spelling, and it is enough:
 * `resampler.reset()` dispatches through the vptr, which no test fixture here
 * fills in, and would be an indirect call where the object makes a direct one.
 *
 * TWO FURTHER REASONS USED TO BE GIVEN HERE AND ARE NOW FALSE.  Both said the
 * class could not be named at all -- that `V90Resampler.h` brings the OTHER
 * definition of `V90Parameters` (finding F1112) and that declaring the member's
 * type as `V90Resampler` had the same problem one level up.  Writing the
 * lifecycle pair forced that to be solved rather than worked around, because a
 * member typed as the base gets the BASE's constructor and destructor emitted
 * and those are the wrong two symbols; `V90Demodulator.h` now claims the
 * parameter header's include guard and declares the member as what it is.  So
 * `((V90Resampler *)&resampler)->V90Resampler::reset()` WOULD compile today.
 * It is not written, because the first reason stands on its own and the
 * qualified-call spelling is the less obvious of the two.
 *
 * The symbol takes `this` as its first stack argument like every other member
 * here (finding F215), and `ResamplerTimingOffset` is `V90Resampler`'s base at
 * offset zero, so the address is the same one `setTimingOffset` is already
 * called on.
 */
extern void v90resampler_reset(ResamplerTimingOffset *self)
	asm("_ZN12V90Resampler5resetEv");

/*
 * `sessionTermination`'s two summaries of the resampler's timing history,
 * named the same way and for the same reason.  Both are NON-virtual and both
 * are called directly on the embedded object's address, and both read
 * `V90Resampler`'s own +0xa4 and +0xa8 -- the history and its length -- which
 * is why the address handed over must be the base subobject's and not a copy.
 */
extern float v90resampler_timingHistoryMean(ResamplerTimingOffset *self)
	asm("_ZN12V90Resampler20getTimingHistoryMeanEv");
extern float v90resampler_timingHistoryStd(ResamplerTimingOffset *self)
	asm("_ZN12V90Resampler19getTimingHistoryStdEv");

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
		resampler.setTimingOffset(V90PF(params)[PARAMS_TIMING_OFFSET]);
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
	phase3Demodulator->word_410 = quickConnect;

	equalizer->enterPhase3();

	errorEnergyPrintPeriod = V90PW(params)[PARAMS_WORD_264];
	timingOffsetPrintPeriod = V90PW(params)[PARAMS_WORD_278];

	/*
	 * Four words of the evaluator, all zero.  The blob writes them
	 * +0x84, +0x70, +0x88, +0x74; they are in offset order here because
	 * four stores of the same constant to four distinct members cannot be
	 * told apart by their order.
	 */
	connectionEvaluator->avePdsnr = 0;
	connectionEvaluator->avePdsnrNofSymbols = 0;
	connectionEvaluator->word_84 = 0;
	connectionEvaluator->word_88 = 0;

	byte_280 = 0;

	/*
	 * `mov (%edx),%ebx; cmpb $0x0,0x2(%ebx); js` -- a pointer out of the
	 * parameter block's first word, and a SIGNED byte two in from it.  The
	 * class it points at is not modelled and this is the only thing in
	 * wave 2 that reaches it.
	 */
	block = *(const signed char *const *)&V90PB(params)[0];
	if (block[2] < 0)
		return;

	if (sessionFlag != 0) {
		edprintf("V90Demodulator: we got PCM upstream under V.92Lite, "
			 "retraining to V.34 upstream...\r\n");
		word_3c = 0x20;
	}
}

/*
 * sessionTermination -- decide whether this call's timing offset is worth
 * keeping, and either write it into the registry or say why not.
 *
 * ONE STORE AND FIVE DIAGNOSTICS, and the store is the point.  Everything
 * else is a reason, printed: the connection was not in the data state, or it
 * was EIA-6, or the evaluation is switched off, or the offset was too noisy.
 * `V90Phase3Demodulator::clearVerificationStatus` runs on every path,
 * including all four refusals.
 *
 * WHAT IT WRITES, AND WHERE.  `(int)(1000.0f * mean)` goes to +0x4c of the
 * `_tagModemParameters` the parameter block's first word points at -- NOT
 * into the parameter block.  `V90PreFilter::setParamEia6` reads that same
 * word back as "prev params ClockDeviation" and multiplies it by 0.001f, so
 * the two functions are the write and the read of one persisted number and
 * the scale factors are reciprocal.  Finding F1274.
 *
 * `+0x34` IS A STATE, NOT A LATCH, and this function is what says so.  The
 * diagnostic below prints `isDataState = %d` for `+0x34 == 3`, and
 * `enterChannelVerification` sets the same field to 5; the name `inPhase3` in
 * the header dates from `enterPhase3`, which returns early when it is exactly
 * 1.  The name is left alone -- it is invented either way (finding F226) and
 * eight parallel worktrees share the header -- and corrected here.  Finding
 * F1273.
 *
 * `isV90WithEia6()` IS CALLED TWICE, and the second call is kept because the
 * blob makes it.  The `if` tests it and the `else` prints it, and between the
 * two nothing runs at all -- so caching it would be equivalent and the
 * mutation that does so is uncaught.  Same shape and same reason as
 * `enterPhase3`'s double call, finding F293.  `+0x34 == 3` is likewise
 * recomputed for the printed argument.
 *
 * THE RETURN TYPE IS NOT `void`, AND IT IS NOT DECIDABLE FURTHER.  The single
 * exit is
 *
 *     1ab75  83 c4 34     add    $0x34,%esp
 *     1ab78  31 c0        xor    %eax,%eax
 *     1ab7a  5b 5e c3     pop; pop; ret
 *
 * and a `void` member emits no `xor` at all where `int` and `unsigned int`
 * both emit exactly that one and are byte-identical to each other.  That was
 * measured on THIS TREE'S g++ under `-m32 -O2 -fomit-frame-pointer
 * -march=i386 -mtune=i686 -mfpmath=387`, and NOT in `tools/toolchain/`'s GCC
 * 3.4.2 container, which is what `getBitRate`'s comment above means by the
 * same phrase -- so the experiment is weaker than that one by exactly that
 * much.  The conclusion survives the difference: a `void` function leaving a
 * dead register clear behind is not a thing any GCC does at -O2, and the
 * positive direction is the ABI's rather than any pass's.  So the constant
 * zero is forced and its signedness is not; `int` is written and the
 * ambiguity is recorded rather than hidden.  Nothing in the tree calls this
 * method, so the choice binds no caller.
 *
 * THE `%c%d.%04d` SHAPE IS THE ONE V90PreFilter.cpp ALREADY CARRIES: a sign
 * character, the truncated magnitude of `x`, and the first four decimals as
 * `|(int)((x - (int)x) * 10000.0f)|`.  Two details of it are the object's
 * rather than the idiom's -- the sign, below, and the fractional part being
 * truncated twice rather than rounded.  10000.0f is loaded once for both
 * calls and spilled as a `double`, which is GCC hoisting one constant and not
 * two different ones.
 *
 * THE SIGN IS `!(0.0f >= x)` AND NOT `0.0f < x`, AND A NaN IS THE DIFFERENCE.
 * 0x1ac24 is `fldz`, so the ZERO is in %st(0) and the value is `fcomps`'s
 * memory operand; the character is then built with NO BRANCH AT ALL --
 *
 *	fldz / fcomps mean / fnstsw %ax / sahf
 *	sbb %eax,%eax / and $0xfffffffe,%eax / add $0x2d,%eax    0x1ac3a
 *
 * -- which is `0x2d - 2*CF`, '+' exactly when the compare set CF.  FCOM sets
 * CF for LESS-THAN and for UNORDERED both, so the object prints '+' for an
 * unordered mean where `0.0f < mean` prints '-'; both zeros print '-'.  A
 * branchless select is only encodable when the TRUE arm is the CF one, which
 * is what fixes the negation on the outside and the zero on the left.  This
 * used to read "so a NaN prints '-'", which was the branch form's answer and
 * not this one's; `st_value` pattern 9 in t_v90demod.cpp is the input that
 * tells them apart, and the two mutations there die on it.  Findings F2300 and
 * F2410, and `ADID_PRINT_SIGN` in V90AutoDigitalImpDetector.cpp is the same
 * reading.
 */
int
V90Demodulator::sessionTermination()
{
	if (inPhase3 == 3 && !preFilter.isV90WithEia6()) {
		if (V90PW(params)[PARAMS_TIMING_HISTORY_EVAL] != 0) {
			float mean = v90resampler_timingHistoryMean(&resampler);
			float std = v90resampler_timingHistoryStd(&resampler);
			int frac;

			frac = (int)((mean - (float)(int)mean) * 10000.0f);
			edprintf("V90Demodulator on sessionTermination: mean "
				 "of timing offset History  = %c%d.%04d\r\n",
				 !(0.0f >= mean) ? '+' : '-',
				 (int)__builtin_fabsf(mean),
				 (frac < 0) ? -frac : frac);

			frac = (int)((std - (float)(int)std) * 10000.0f);
			edprintf("V90Demodulator on sessionTermination: std "
				 "of timing offset History  = %c%d.%04d\r\n",
				 !(0.0f >= std) ? '+' : '-',
				 (int)__builtin_fabsf(std),
				 (frac < 0) ? -frac : frac);

			edprintf("V90Demodulator on sessionTermination: "
				 "1000* std = %d\r\n", (int)(std * 1000.0f));

			/*
			 * The threshold is on the LEFT of the comparison in
			 * the object -- `flds 0x16c(%ecx); fcomps std; jb` --
			 * so the arm that saves is the one where the
			 * parameter is at least as large as the deviation.
			 * Its name reads the other way round: the author
			 * called it TIMING_OFFESET_MIN_STD_FOR_SAVE and uses
			 * it as a maximum.
			 */
			if (V90PF(params)[PARAMS_MIN_STD_FOR_SAVE] >= std) {
				int *modemParams;

				edprintf("V90Demodulator on "
					 "sessionTermination: Timing offset "
					 "saved in Registry!\r\n");

				modemParams = *(int *const *)&V90PB(params)[0];
				modemParams[MODEM_CLOCK_DEVIATION] =
				    (int)(1000.0f * mean);
			}
		} else {
			/*
			 * The one message in this function that ends in a
			 * bare "\n" rather than "\r\n".  It is the object's
			 * and it is reproduced rather than tidied; D200.
			 */
			edprintf("V90Demodulator on sessionTermination: "
				 "Timing offset NOT saved to registry, "
				 "EVALUATION DISABLED !\n");
		}
	} else {
		edprintf("V90Demodulator on sessionTermination: Timing offset "
			 "NOT saved to registry (isDataState = %d, "
			 "isEia6 = %d)\r\n",
			 inPhase3 == 3, preFilter.isV90WithEia6());
	}

	phase3Demodulator->clearVerificationStatus();

	return 0;
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
 * ===========================================================================
 * THE PHASE-4 AND DATA-PHASE ENTRIES
 * ===========================================================================
 *
 * Five members that move `inPhase3` on, and they share one shape which is
 * worth reading once rather than five times:
 *
 *   - EVERY ONE OF THEM RETURNS IMMEDIATELY IF IT IS ALREADY IN THE STATE IT
 *     IS ENTERING, and the test is for EQUALITY with that state and not for
 *     "already past it".  `enterRRN`, `enterFPE` and `enterPhase4` all test
 *     against 2 and all set 2, so the three are three ways into ONE state and
 *     any of them suppresses the other two.  That is the object's and it is
 *     not obviously what was meant; it is reproduced, not tidied.
 *
 *   - THE DIAGNOSTIC IS BEFORE THE WORK, which is visible only because the
 *     guard is compiled inline: `cmpl $0x1,dsplibs_debug_level; ja <away>`
 *     sits between the state test and the first store, and the away block
 *     prints and jumps BACK to the store.  So the source is
 *     `if (DSPLIB_DEBUG_ON()) dsplibs_debug_printf(...);` and then the work,
 *     and writing it the other way round moves the branch.  Same reading as
 *     v34hshak.h's note on `DSPLIB_DEBUG_ON()` printing before it stores.
 *
 *   - +0x048 IS A DEADLINE BUILT FROM THE ROUND-TRIP DELAY with one `lea`
 *     each, and V90Demodulator.h carries the derivation and the reason it is
 *     not named further.
 */

/*
 * enterRRN -- 225 bytes.  The rate-renegotiation entry into phase 4.
 *
 * TWO THINGS IT DOES THAT ITS TWO SIBLINGS DO NOT.
 *
 * 1. IT RECORDS A THREE-WAY CONJUNCTION IN `tagV90AdditionalCPinfo`, AND THE
 *    DESTINATION IS NOT THE OBJECT THE CONDITION IS READ FROM.  The address
 *    is loaded at 0x1b5bb -- `mov 0x20(%ebx),%ecx`, which is +0x20 and so
 *    `additionalCPinfo` -- while the first term of the condition comes from
 *    +0x20c, the connection evaluator, and the other two from +0x1e0.  Three
 *    different objects, and the destination is the one nothing in the
 *    condition mentions.  The load is hoisted ABOVE the tests because it is
 *    needed on every path.
 *
 *    The object loads `connectionEvaluator->word_90`, and only if that is
 *    non-zero loads `phase4Demodulator`'s +0x3c and +0x38 -- and then stores
 *    ZERO whichever way every one of those tests went:
 *
 *        1b5be  test %eax,%eax ; je 1b5d0        -> store 0
 *        1b5cd  jne 1b623                        -> 1b623 sets %edx = 1 ...
 *        1b62d  jne 1b5d2                        -> ... and stores %edx
 *        1b62f  jmp 1b5d0                        -> store 0
 *
 *    but 1b5d0 is `xor %edx,%edx` immediately before 1b5d2, so the value
 *    reaching the store is 1 on exactly one path and 0 on the other three.
 *    The `mov $0x1,%edx` at 1b626 is therefore live and this is a genuine
 *    three-way `&&`, not a fold -- written below as one condition.  The
 *    register holding the destination is loaded at 1b5bb, BEFORE the tests,
 *    which is the compiler hoisting an address it needs on every path.
 *
 * 2. IT CLEARS `byte_280`, WHICH IS WHAT `getBitRate` GATES ON.  So after
 *    `enterRRN` the reported rate is 0 until something sets it again.  That
 *    is the one observable coupling between this member and the diagnostics
 *    below, and `t_v90dataph.cpp` drives it.
 */
void
V90Demodulator::enterRRN()
{
	if (inPhase3 == 2)
		return;

	if (DSPLIB_DEBUG_ON())
		dsplibs_debug_printf("V90Demodulator: RRN detected: "
				     "enter Phase 4\r\n");

	word_38 = 0;
	word_44 = 0;
	inPhase3 = 2;
	word_48 = 0x10680 + 2 * (unsigned int)phase2Info->rtd;

	errorEnergyPrintPeriod = V90PW(params)[PARAMS_WORD_268];
	timingOffsetPrintPeriod = V90PW(params)[PARAMS_WORD_27C];

	additionalCPinfo->word_10 =
	    (connectionEvaluator->word_90 != 0 &&
	     phase4Demodulator->int_003c != 0 &&
	     phase4Demodulator->int_0038 != 0) ? 1 : 0;

	byte_280 = 0;
	demapper->linearMappStudyEnabled = 0;

	if (DSPLIB_DEBUG_ON())
		dsplibs_debug_printf("V90Demodulator: disable linear mapping "
				     "study\n");
}

/*
 * enterFPE -- 110 bytes.  `enterRRN` without the connection-evaluator
 * condition, without the `byte_280` clear and without the second diagnostic:
 * the state, the counters, the deadline and the two parameter copies, and
 * nothing else.
 */
void
V90Demodulator::enterFPE()
{
	if (inPhase3 == 2)
		return;

	if (DSPLIB_DEBUG_ON())
		dsplibs_debug_printf("V90Demodulator: FPE detected: "
				     "enter Phase 4\r\n");

	word_38 = 0;
	word_44 = 0;
	inPhase3 = 2;
	word_48 = 0x10680 + 2 * (unsigned int)phase2Info->rtd;

	errorEnergyPrintPeriod = V90PW(params)[PARAMS_WORD_268];
	timingOffsetPrintPeriod = V90PW(params)[PARAMS_WORD_27C];
}

/*
 * enterPhase4 -- 110 bytes, and the same size as `enterFPE` for a reason: it
 * is the same member with two differences, both forced.
 *
 * `word_44` is ACCUMULATED rather than cleared -- `mov 0x38(%ebx),%eax; add
 * %eax,0x44(%ebx)` before +0x38 is zeroed, so this entry adds the count it
 * found to the running total where the other two discard it -- and the
 * deadline's `lea` is `0x28230(%edx,%edx,4)`, five times the round-trip delay
 * from a larger base, where the other two use twice it.
 *
 * THE ORDER OF THE +0x38 READ AND THE +0x44 ADD IS NOT FREE.  The object
 * reads +0x38 at 0x1b2ce, adds at 0x1b2d4 and stores zero at 0x1b2da; writing
 * the clear first would change what is added.
 *
 * AND THE CLEAR IS WRITTEN AFTER THE DEADLINE, WHICH IS DECODED (7770's
 * argument) RATHER THAN TRANSCRIBED.  The object emits the zero store BETWEEN
 * the `rtd` load and the deadline's `lea`, and it emits the deadline's own
 * store one slot later than we did -- fifteen bytes that read like two
 * independent scheduling differences and are one statement.  Fifteen cells
 * were compiled before any was read: all twelve orders of `inPhase3 = 2`,
 * `word_44 += word_38`, `word_38 = 0` and the deadline that keep the
 * accumulate ahead of the clear, plus three that sink the clear past the two
 * parameter copies or swap them.  Differing bytes of 110:
 *
 *     inPhase3 acc clr w48  15     acc clr inPhase3 w48  15     w48 ... 38, 38, 38
 *     inPhase3 acc w48 clr   0 <-- acc clr w48 inPhase3  10     clr after +0x288  23
 *     inPhase3 w48 acc clr  26     acc w48 inPhase3 clr  18     clr last          32
 *     acc inPhase3 clr w48  15     acc w48 clr inPhase3  20     +288/+290 swapped  4
 *     acc inPhase3 w48 clr   0 <--
 *
 * **TWO CELLS REACH ZERO, so the preimage is NOT unique and this is a
 * decoding of ONE fact and not of the whole order.**  What both hits agree on
 * -- and what every other cell in the family contradicts -- is that
 * `word_38 = 0;` stands IMMEDIATELY AFTER the deadline: moving it one slot
 * earlier costs 15 bytes and sinking it past either parameter copy costs 23
 * or 32.  What they disagree on is where `inPhase3 = 2;` goes, first or
 * second, and GCC 3.4.2 emits both identically, so the object cannot say.
 * It is left where it was, which is the smaller claim.  7771's rule is the
 * reason this is written as one recovered statement position rather than as
 * a recovered statement order.
 *
 * (Note the near miss at 4, the parameter copies swapped.  The enumeration
 * was run to all fifteen before any cell was read -- 7779 declined a 2-of-387
 * for exactly this reason, and 4 is more tempting than 27.)
 */
void
V90Demodulator::enterPhase4()
{
	if (inPhase3 == 2)
		return;

	if (DSPLIB_DEBUG_ON())
		dsplibs_debug_printf("V90Demodulator: enter Phase 4\r\n");

	inPhase3 = 2;
	word_44 += word_38;
	word_48 = 0x28230 + 5 * (unsigned int)phase2Info->rtd;
	word_38 = 0;

	errorEnergyPrintPeriod = V90PW(params)[PARAMS_WORD_268];
	timingOffsetPrintPeriod = V90PW(params)[PARAMS_WORD_27C];
}

/*
 * `V90Parameters` +0x048.  `tools/vparse.py` gives the author's own name for
 * it; the index spelling is this file's convention (see PARAMS_TIMING_OFFSET
 * above) and not a statement that the named map is unavailable.
 */
#define PARAMS_ANALOG_RATE_MASK	(0x048 / 4)

/*
 * exitPhase3 -- 768 bytes at 0x1bb50, and the hand-over from phase 3 to phase
 * 4.  Six things happen in one straight line with two conditionals in it: the
 * TRN1d RMS ratio is reported and copied into the additional-CP record, the
 * DIL is ended, phase 4 is entered IF phase 3 terminated, the TRN2
 * constellations are designed, the design's failure raises a delayed retrain,
 * and the phase 4 demodulator is reset.
 *
 * THE LATCH IS `== 1` AND NOT `!= 0`, exactly as `enterPhase3`'s is
 * (`cmpl $0x1,0x34(%edi); je` at 0x1bb67, with the JE going INTO the body):
 * this member does its work only from state 1, and `enterChannelVerification`
 * puts the object in state 5 where a `!= 0` reading would let it through.
 *
 * `enterPhase4()` IS CALLED, NOT REPEATED.  0x1bdd0..0x1be1d is that member's
 * 110 bytes instruction for instruction -- the `inPhase3 == 2` test, the
 * gated "enter Phase 4" line, the `word_44 += word_38` accumulation and the
 * `lea 0x28230(%edx,%edx,4)` deadline -- inlined by `-O3
 * -finline-functions`.  Its idempotence test can never fail HERE, because the
 * only way in is `inPhase3 == 1`; it is still the callee's test and not a
 * dead branch of this function.
 *
 * AND THE EQUALISER IS ENTERED IN THE SAME ARM, WHICH THE INLINING HIDES.
 * `je 1be1e` at 0x1bdd4 -- the idempotence test's TAKEN edge -- lands on
 * `mov 0x1d8(%edi),%edx ; call V90Equalizer::enterPhase4` and not on the join
 * with the common tail, so both arms of `inPhase3 == 2` converge on that call
 * and it is the second statement of the `word_30 == 0x14` block rather than
 * part of the member that was inlined.  The first draft of this function
 * missed it and read as complete: the branch structure, every store and every
 * other call agreed, and what said otherwise was the instruction COUNT, 174
 * against 186.
 *
 * THE THREE PRINTED NUMBERS NEVER REACH MEMORY, so the transcript is their
 * only witness.  The sign character is the branchless `0x2d - 2*CF` form
 * `sessionTermination` documents at length, and it compares the field that
 * was JUST STORED rather than the local -- `mov 0x20(%edi),%ecx ; fcomps
 * 0x8(%ecx)` at 0x1bc00, a reload of both the pointer and the value.  The
 * magnitude is `(int)fabsf` of the LOCAL and the fraction is scaled by
 * `1.0e8f` out of `.rodata.cst4+0x11c` (read, not inferred from `%08d`).
 *
 * THE TWELVE-ARGUMENT DESIGN CALL IS FOUR VIEWS OF ONE OBJECT.  Arguments 2
 * to 5 are `autoDigitalImpDetector` plus 0, 0x600, 0xd00 and 0x2800 -- the
 * four per-code tables `V90AutoDigitalImpDetector.h` already names -- built
 * as `add $0x2800,%edx` on four separate reloads of the same pointer, which
 * is what an array member's address looks like and not four pointers being
 * carried.  Arguments 6, 7 and 8 are three scalars out of the tail of the
 * same object, and argument 9 is `getMaxUcode()`, which returns the address
 * of a fifth (`maxUcode[6]` at +0xa956).  So five of the twelve are that one
 * detector, and the designer never sees the object itself.
 *
 * ARGUMENT 12 IS AN EMBEDDED FIELD AND WAS THE ONE THING THE CALL COULD HAVE
 * GOT WRONG.  `mov 0x238(%edi),%ebx` is `spectralVerifier.word_28` --
 * +0x210 + 0x28 -- and it is loaded BEFORE the two intervening calls and
 * stashed at 0x40(%esp), which is what a value read early and used late looks
 * like.  It is the `V90SpecialSpectralConditions` the verifier detected, so
 * the detector's answer reaches the constellation designer through this
 * member and through nothing else.
 *
 * `sessionFlag` PICKS THE LOOKAHEAD AND NOTHING ELSE HERE.  Non-zero takes
 * `jdV92`, zero takes `jd`, and both members return an `unsigned char` which
 * the call widens with `movzbl`.
 *
 * THE FAILURE ARM RAISES A DELAYED RETRAIN.  A zero from `V90TRN2Design`
 * stores 1 into `connectionEvaluator->delayedRetrainRequest` and prints, at
 * level > 1, "V90Demodulator::exitPhase3() delayedRetrainRequest !!!" --
 * which is the author's own name for that slot and the third function to
 * touch the pair.  See V90ConnectionEvaluator.h for the derivation and
 * finding F9480 for the naming pass that carried it into the field.
 */
void
V90Demodulator::exitPhase3()
{
	float ratio;
	int whole, frac;

	if (inPhase3 != 1)
		return;

	ratio = trn1dRmsRatio;
	additionalCPinfo->float_08 = ratio;
	additionalCPinfo->word_00 = 0;
	additionalCPinfo->word_04 = 0;

	whole = (int)ratio;
	frac = (int)((ratio - (float)whole) * 1.0e8f);
	edprintf("V90Demodulator: TRN1d RMS Ratio = %c%d.%08d\r\n",
		 !(0.0f >= additionalCPinfo->float_08) ? '+' : '-',
		 (int)__builtin_fabsf(ratio),
		 (frac < 0) ? -frac : frac);

	additionalCPinfo->word_0c = autoDigitalImpDetector->int_a960;
	additionalCPinfo->word_10 = 0;
	additionalCPinfo->short_14 =
	    (short)V90PW(params)[PARAMS_ANALOG_RATE_MASK];

	phase3Demodulator->exitDIL();

	if (phase3Demodulator->word_30 == 0x14) {
		enterPhase4();
		equalizer->enterPhase4();
	}

	trn2Designer->setNofUcodesInTrn2(
	    (short)autoDigitalImpDetector->isThereAnyAltRbsPhase());

	if (trn2Designer->V90TRN2Design(
		mappingParams,
		autoDigitalImpDetector->linMapp,
		autoDigitalImpDetector->linMappAlt,
		autoDigitalImpDetector->byte_0d00,
		autoDigitalImpDetector->short_2800,
		autoDigitalImpDetector->pcmType,
		(PcmType)autoDigitalImpDetector->int_a960,
		autoDigitalImpDetector->unSuspectedPhase,
		phase3Demodulator->getMaxUcode(),
		sessionFlag != 0 ? jdV92->getMaxLookahead()
				 : jd->getMaxLookahead(),
		(unsigned char)(phase2Info->maxTxPower + 1),
		(V90SpecialSpectralConditions)spectralVerifier.word_28) == 0) {
		if (DSPLIB_DEBUG_ON())
			dsplibs_debug_printf("V90Demodulator::exitPhase3() "
					     "delayedRetrainRequest !!!\r\n");
		connectionEvaluator->delayedRetrainRequest = 1;
	}

	edprintf("V90Demodulator: TRN2d spectral parameters:\r\n");
	displaySpectralParams(mappingParams);

	phase4Demodulator->reset(phase2Info->Uinfo,
				 (Phase4DemodulatorState)0, 0, quickConnect);
}

/*
 * enterDataPhase -- 322 bytes.  State 3, and the point at which the linear
 * mapping study is armed for the data phase.
 *
 * THE STUDY LENGTH IS A CHOICE BETWEEN TWO LITERALS AND NEITHER IS A POINTER.
 * The object has
 *
 *     1bec9  ba 38 31 00 00   mov $0x3138,%edx
 *     1bf70  b8 30 75 00 00   mov $0x7530,%eax
 *
 * and NEITHER line carries a relocation, which is the whole of finding F245's
 * point: they are the integers 12600 and 30000 and not offsets into
 * `.rodata`.  12600 when `quickConnect` is set and 30000 when it is not, and
 * `quickConnect` is the field `reset` stores its argument into -- so a quick
 * connect studies for the shorter run.  That reading is the field's provenance and
 * not this function's, which only picks between two numbers.
 *
 * THE RATE DIAGNOSTIC IS `getBitRate()` AND NOT A COPY OF IT.  0x1bf06 is the
 * same `cmpb $0x0,0x280` guard, the same `imul $0x1f40`, the same 1/6 and the
 * same `+ 0.5f` under the same rounding-mode dance as the out-of-line member
 * at 0x1b8e0, inlined here because it is small and in the same translation
 * unit.  Writing the expression out again would be a second place to get it
 * wrong; calling it is what the source did and what reproduces the code.
 */
void
V90Demodulator::enterDataPhase()
{
	if (inPhase3 == 3)
		return;

	word_38 = 0;
	inPhase3 = 3;
	word_3c = 0x1e;

	errorEnergyPrintPeriod = V90PW(params)[PARAMS_WORD_26C];
	timingOffsetPrintPeriod = V90PW(params)[PARAMS_WORD_280];

	resampler.setBllState(V90_BLL_STEADY_STATE, 1);

	if (DSPLIB_DEBUG_ON())
		dsplibs_debug_printf("V90Demodulator: enter Data Phase, "
				     "Rate = %d [bps]\r\n", getBitRate());

	demapper->resetLinearMappStudy(quickConnect != 0 ? 12600u : 30000u);
	demapper->linearMappStudyEnabled = 1;

	if (DSPLIB_DEBUG_ON())
		dsplibs_debug_printf("V90Demodulator: reset and enable linear "
				     "mapping study in data\n");
}

/*
 * enterDataSteadyState -- 548 bytes, and 400 of them are one diagnostic that
 * `sessionTermination` above already carries word for word.
 *
 * IT IS `sessionTermination`'S TIMING-HISTORY BLOCK WITH A DIFFERENT GUARD.
 * Same two resampler accessors, same `%c%d.%04d` split, same `1000* std`
 * line, same `PARAMS_MIN_STD_FOR_SAVE >= std` test with the parameter on the
 * LEFT, and the same store into the modem parameter block's
 * `MODEM_CLOCK_DEVIATION`.  Everything that comment says about the sign
 * character applies here unchanged and is not repeated: `fldz` puts the ZERO
 * in %st(0), the character is `0x2d - 2*CF` with no branch, and an unordered
 * value therefore prints '+'.  Findings F2300 and F2410.
 *
 * WHAT IS DIFFERENT IS THE GUARD, AND IT IS NOT A PURE TEST.  This member
 * COPIES the evaluation flag into `word_278` and then branches on it:
 *
 *     1b345  mov  0x160(%edx),%eax
 *     1b34b  test %eax,%eax
 *     1b34d  mov  %eax,0x278(%esi)      <- the store is between them
 *     1b353  jne  ...
 *
 * -- the store is scheduled into the middle of the test, which is free, but
 * the store itself is not: `sessionTermination` reads the same parameter and
 * does NOT copy it.  So the assignment is in the source and the value the
 * branch uses is the value that was stored.
 *
 * AND THE ELSE ARM IS EMPTY, where `sessionTermination`'s prints "EVALUATION
 * DISABLED".  There is no second string and no second call in the range; the
 * function falls straight to the shared tail.
 */
void
V90Demodulator::enterDataSteadyState()
{
	if (inPhase3 == 4)
		return;

	inPhase3 = 4;
	edprintf("V90Demodulator: enter Data steady state\r\n");

	word_278 = V90PW(params)[PARAMS_TIMING_HISTORY_EVAL];
	if (word_278 != 0) {
		float mean = v90resampler_timingHistoryMean(&resampler);
		float std = v90resampler_timingHistoryStd(&resampler);
		int frac;

		frac = (int)((mean - (float)(int)mean) * 10000.0f);
		edprintf("V90Demodulator: mean of timing offset History  = "
			 "%c%d.%04d\r\n",
			 !(0.0f >= mean) ? '+' : '-',
			 (int)__builtin_fabsf(mean),
			 (frac < 0) ? -frac : frac);

		frac = (int)((std - (float)(int)std) * 10000.0f);
		edprintf("V90Demodulator: std of timing offset History  = "
			 "%c%d.%04d\r\n",
			 !(0.0f >= std) ? '+' : '-',
			 (int)__builtin_fabsf(std),
			 (frac < 0) ? -frac : frac);

		edprintf("V90Demodulator: 1000* std = %d\r\n",
			 (int)(std * 1000.0f));

		if (V90PF(params)[PARAMS_MIN_STD_FOR_SAVE] >= std) {
			int *modemParams;

			edprintf("V90Demodulator: Timing offset saved in "
				 "Registry!\r\n");

			modemParams = *(int *const *)&V90PB(params)[0];
			modemParams[MODEM_CLOCK_DEVIATION] =
			    (int)(1000.0f * mean);
		}
	}

	demapper->linearMappStudyEnabled = 0;

	if (DSPLIB_DEBUG_ON())
		dsplibs_debug_printf("V90Demodulator: disable linear mapping "
				     "study.\n");
}

/*
 * ===========================================================================
 * THE DIAGNOSTICS ACCESSORS
 * ===========================================================================
 */

/*
 * log10() on the coprocessor, as the object computes it.
 *
 * `fldlg2` pushes log10(2) at the register's full 64-bit mantissa and `fyl2x`
 * computes st(1) * log2(st(0)) and pops, so the sequence takes one value and
 * leaves one -- net stack effect zero, which is what makes the "=t"/"0" tie
 * legal.  glibc's log10() is a polynomial and differs from this in the last
 * place often enough to matter once the result is scaled by ten, which is
 * exactly what `getAT_UD` does to both of its results.
 *
 * THIS IS THE THIRD COPY of the same four-line helper -- `x87_log10` in
 * V90Equalizer.cpp, `psd_x87_log10` in Psd.cpp, `trn2_x87_log10` in
 * V90TRN2Designer.cpp and the one in VPcmFloModem.cpp -- and it is a copy
 * deliberately, on the reasoning VPcmFloModem.cpp already wrote down:
 * hoisting it into a shared header from a worktree touches files other
 * batches own for no behavioural gain.  Recorded so a later cleanup can
 * collapse them all at once.
 */
static inline long double
dem_x87_log10(long double x)
{
	long double r;

	__asm__ ("fldlg2\n\tfxch %%st(1)\n\tfyl2x" : "=t" (r) : "0" (x));
	return r;
}

/*
 * getRbsPattern -- 51 bytes, and the whole of it is one loop.
 *
 * Six bytes of `V90AutoDigitalImpDetector::byte_280c` widened into six words
 * of the caller's array.  The count is the object's `cmp $0x5 / jbe` and is
 * `V90ADID_PHASES`, which is the array's own declared length -- so the loop
 * is written against that constant rather than a literal 6, and the two are
 * required to agree.
 *
 * THE ENTRY JUMP IS THE COMPILER'S, NOT A CONDITION.  0x1b941 is `jmp 1b950`
 * into the top of the loop body, which is what GCC emits when it has proved
 * the loop runs at least once and still wants the body aligned.  There is no
 * zero-trip test in the source and none is written here.
 */
void
V90Demodulator::getRbsPattern(unsigned int *rbs) const
{
	unsigned int i;

	for (i = 0; i < V90ADID_PHASES; i++)
		rbs[i] = autoDigitalImpDetector->byte_280c[i];
}

/*
 * indicateRemoteRateReneg -- 40 bytes.  One call and one store.
 *
 * The two go through DIFFERENT pointers and that is the whole of the
 * function's content: the call is on +0x20c, the connection evaluator, and
 * the store is into +0x208 -- `constellationDesigner` -- at its +0x48.  The
 * object loads the second pointer AFTER the call returns, so the call is not
 * what supplies it, and the two objects are not confusable even though the
 * fields are adjacent.
 *
 * `V90ConstellationDesigner`'s +0x48 is `word_48`, "zeroed by reset", and
 * this is the only place in the object that ever stores a 1 into it.  It
 * keeps its offset name: one writer storing a literal 1 says the field is a
 * flag and says nothing about what of.
 */
void
V90Demodulator::indicateRemoteRateReneg() const
{
	connectionEvaluator->indicateRemoteRateReneg();
	constellationDesigner->word_48 = 1;
}

/*
 * getAT_UD -- 418 bytes, and the only place in the object that names
 * `TAG_DiagnosticResults`.  See dsplib/TAG_DiagnosticResults.h for what is
 * modelled of that record and what is deliberately left padded.
 *
 * FOUR THINGS IN IT ARE FORCED AND ARE WRITTEN THE OBJECT'S WAY.
 *
 * 1. THE ROUND-TRIP DELAY IS DIVIDED UNSIGNED, AND THAT IS A TYPE STATEMENT.
 *    The object computes `rtd * 10` with `lea (%edx,%edx,4)` then `add
 *    %ebx,%ebx`, and divides by 96 with the unsigned reciprocal
 *
 *        mov $0xaaaaaaab,%eax ; mul %ebx ; shr $0x6,%edx
 *
 *    -- `mul`, not `imul`, and with NO sign correction anywhere: a signed
 *    divide by 96 needs the quotient adjusting for a negative dividend and
 *    there is no `cltd`, no `sar` and no conditional add in the range.  So
 *    the arithmetic is unsigned, while `V90Phase2Info::rtd` is declared
 *    `int` with a comment saying its signedness is not recoverable because
 *    "nothing does arithmetic on it".  Something does now.
 *
 *    THE CAST IS HERE RATHER THAN IN THE FIELD, and that is a decision and
 *    not an oversight.  Retyping `rtd` would reach a header three other
 *    live branches include, which is exactly finding F3511's shape; the cast
 *    reproduces the object's instructions today and the evidence for the
 *    retype is recorded in the finding for a pass that owns that header.
 *
 * 2. `+0x074` IS COPIED WITH AN INTEGER `mov` AND IS STILL A FLOAT.  0x1ba3e
 *    loads the equaliser's +0x80 into %eax and stores it, and 0x1ba47 then
 *    loads the SAME address with `flds` for the logarithm.  A float-to-float
 *    assignment with no conversion is a 32-bit `mov` in this compiler, so the
 *    integer move is not evidence of an integer field -- and the independent
 *    writer in `VPcmV34GetDiagnostics` stores that offset with `fsts`, which
 *    settles it.  CLAUDE.md's free column: the instruction was the
 *    compiler's to choose.
 *
 * 3. THE TWO dB FIGURES SHARE ONE CONSTANT.  `10.0f` is loaded once, at
 *    0x1ba53, and left on the stack across the second `fyl2x` -- `fmul
 *    %st,%st(1)` for the first result and `fmulp` for the second.  That is
 *    the compiler hoisting one constant out of two expressions, so the
 *    source has the literal twice.  Both are `10.0f * log10f(x)`, built as
 *    `log10(2) * log2(x)` by `fldlg2 / fyl2x`, which is how this compiler
 *    open-codes `log10f`.
 *
 *    TWO CLAIMS THAT USED TO BE HERE ARE WRONG AND THE MEASUREMENT IS IN
 *    F8061.  "Writing it once into a local moves the code" -- it does not;
 *    that cell is byte-identical to this one.  And WE DO NOT EMIT THE
 *    ARRANGEMENT DESCRIBED ABOVE: we load the constant BEFORE the first
 *    `fyl2x` and multiply `fmul %st(1),%st`, which needs no `fxch`, so we
 *    are one instruction short of the blob and 84 of 418 bytes differ.
 *    Fifteen spellings of these two expressions give ONE emission and
 *    eighteen cells of statement order crossed with the helper's `asm`
 *    formulation give twelve, of which exactly one reaches the object -- and
 *    that one is `__asm__ __volatile__`, declined as fitting the compiler.
 *    A two-arm model isolates it: the same two expressions over this helper
 *    emit OUR arrangement with the plain `asm` and the BLOB's with the
 *    volatile one, nothing else differing.  So the residual belongs to the
 *    stand-in below rather than to the source here, and the paragraph above
 *    describes the OBJECT and not this file's output.
 *
 * 4. THE RBS PACKING IS LSB-FIRST AND THE OBJECT PROVES THE ORDER.  Five
 *    `lea (%r,%r,2)` steps fold the six bytes from the TOP down --
 *    `b[0] + 2*(b[1] + 2*(b[2] + 2*(b[3] + 2*(b[4] + 2*b[5]))))` -- so
 *    `byte_280c[0]` is bit 0.  The six are read into a local array first and
 *    the array is then passed to `edprintf` element by element, which is why
 *    `getRbsPattern` is called for its side effect rather than the six being
 *    read twice: the object reads +0x280c exactly six times in this function.
 */
void
V90Demodulator::getAT_UD(TAG_DiagnosticResults *results) const
{
	unsigned int rbs[V90ADID_PHASES];

	results->word_0ec = word_264;
	results->word_0f0 = word_268;
	results->word_0f4 = word_26c;

	results->dataRate = getBitRate();

	/*
	 * A PCM receiver has no carrier and its symbol rate is 8000, which is
	 * exactly what `VPcmV34GetCurrentRxCarrier` and
	 * `...GetCurrentRxBaudRate` answer for the same condition.  These two
	 * offsets were `word_0bc` and `word_0b4` when this was written and
	 * are named now; finding F5500 settled the direction.
	 */
	results->rxCarrier = 0;
	results->rxBaudRate = 8000;

	results->float_074 = equalizer->meanErrorEnergyCurrent;
	results->float_070 = (float)(10.0f * dem_x87_log10(
	    (long double)equalizer->meanErrorEnergyCurrent));
	results->float_068 = (float)(10.0f * dem_x87_log10(
	    (long double)agc.level));

	results->roundTripDelay = (unsigned int)phase2Info->rtd * 10u / 96u;

	getRbsPattern(rbs);

	results->rbsPattern = rbs[0] + 2 * (rbs[1] + 2 * (rbs[2] + 2 *
			      (rbs[3] + 2 * (rbs[4] + 2 * rbs[5]))));

	edprintf("V90 Diagnostics\r\n");
	edprintf("-------------------------\r\n");
	edprintf("RBS : %d (%d%d%d%d%d%d)\r\n", results->rbsPattern,
		 rbs[0], rbs[1], rbs[2], rbs[3], rbs[4], rbs[5]);
}

/*
 * ===========================================================================
 * progress -- the receive session, one buffer of samples at a time
 * ===========================================================================
 *
 * `.text+0x1ca90`, 0x1c6c = 7,276 bytes, 1,698 instructions and 129 calls: the
 * largest function in this reconstruction and the last of the
 * `V90Demodulator::progress` batch.  It is the whole receive chain --
 * prefilter, AGC, resampler, equaliser -- followed by a state machine over
 * FOUR dispatch tables, and it is the only caller of most of what the batch
 * before it wrote.
 *
 * THE FOUR TABLES ARE THE MAP, and each was read with `tools/tabdump.py`
 * rather than inferred from the branch layout:
 *
 *     .rodata+0x69c   5 entries   switch (inPhase3)          0 .. 4
 *     .rodata+0x6b0   6 entries   switch (evaluateConnection())
 *     .rodata+0x6c8  22 entries   switch (word_3c)        0x00 .. 0x15
 *     .rodata+0x720  29 entries   switch (word_3c)        0x19 .. 0x35
 *
 * so `inPhase3` selects the PHASE and `word_3c` the state within it: the
 * 0x6c8 table is reached only from `inPhase3 == 1` (phase 3) and the 0x720
 * table only from `inPhase3 == 2` (phase 4).  The two are disjoint in value
 * as well as in reach, which is why one field carries both.
 *
 * `word_3c` IS THE EQUALISER'S ANSWER AND KEEPS ITS OFFSET NAME.  Every call
 * begins `word_3c = equalizer->stateCount`, and `V90Equalizer::process` is
 * what puts 0x23, 0x25 and the rest there -- so the field is an event code
 * rather than a count, and this member is the only reader of it in the
 * object.  Naming it would be one inferential step past what the object
 * proves, which is 3120's rule and 7453's precedent (`P4M_STATE_UNNAMED_14`):
 * the strings below name ARMS, not state values, and no string names a
 * number.  The dispatch is written in the case labels and that is the record.
 *
 * FOUR OF ITS OWN CLASS'S MEMBERS ARE CALLED AND TWO ARE INLINED, and which
 * is which is the compiler's business rather than the source's.
 * `enterDataSteadyState`, `enterRRN`, `enterDataPhase` and `exitPhase3` are
 * out-of-line calls in the blob; `enterPhase4` (0x1dc76) and `enterFPE`
 * (0x1d418) appear inlined instruction for instruction, idempotence test and
 * gated string included.  Both spellings are the same source -- a call --
 * and finding F7480 is why that is worth stating: at this size the instruction
 * COUNT is the only completeness check there is, and a member that inlines on
 * one side and not the other moves it without moving any behaviour.
 *
 * WHAT THE COMMON TAIL DOES, and it runs whatever the phase.  Two independent
 * print-period counter/period pairs (`errorEnergyPrintCounter`/
 * `errorEnergyPrintPeriod` for the error energy, `timingOffsetPrintCounter`/
 * `timingOffsetPrintPeriod` for the timing offset -- named in finding F10132)
 * and, when `word_40` is set, the energy-drop detector: `agc.level` under
 * `ENERGY_DROP_DETECTOR_THRESHOLD` for `NO_ENERGY_DURATION_FOR_REMOTE_RETRAIN`
 * samples raises a remote retrain.  The exit is then one of three counters --
 * `word_264` for 0x23, `word_268` for 0x1f..0x21 and `word_26c` for 0x26 --
 * incremented on the way out, and the return value is not set on any path.
 *
 * THE TIMING-OFFSET LINE CALLS `getTimingOffsetPPM` FOUR TIMES and that is
 * the source and not an artefact: the value is a call rather than a variable,
 * so each of the four spellings in the statement is its own call, and GCC's
 * right-to-left argument evaluation is what puts the fractional part's two
 * first.  The `%c%d.%03d` shape itself is `exitPhase3`'s, reused unchanged.
 *
 * THE `V90MP` SIGNATURE TEST IS SIX FIELD COMPARISONS AND THREE INSTRUCTIONS.
 * `mp->Trellis == 0 && mp->NonLin == 0` is one `cmpw $0x0,0x2(%eax)` and each
 * of the three `h` pairs is one 32-bit `test`, because `fold_truthop` merges
 * comparisons of ADJACENT fields against zero into one wider load.  Written
 * field by field, which is what the object's own widths say the author wrote.
 *
 * ONE HEADER CORRECTION FALLS OUT: `V90Demodulator` +0x274 was `pad_274[4]`,
 * "nothing reads it".  This member both writes it (from
 * `equalizer->meanErrorEnergyMean`, when Ed arrives on a silence RRN) and
 * reads it back (as the constellation designer's third argument, the `f` of
 * its mangling), so it is four bytes and a `float`.  It keeps the offset name:
 * what it holds is "the mean error at the moment the redesign was armed", and
 * that is a role bounded by two sites rather than a meaning the object states.
 */

/*
 * SIX PARAMETER SLOTS THIS MEMBER READS AS `float` WHERE V90Parameters.h
 * DECLARES THEM `int`, and the object's own load width is what says so:
 * `flds` at 0x1deb2 (+0x06c), 0x1def6 (+0x070) and 0x1d2ce (+0x328), and a
 * bare 32-bit `mov` at 0x1e06d, 0x1e079 and 0x1e085 that lands in three slots
 * the header already types `float` (+0x18c, +0x190, +0x194).  A `mov` between
 * two `float`s is a copy; the same statement written between an `int` and a
 * `float` is a CONVERSION and two more instructions, so the view is forced
 * rather than cosmetic.  Reaching them through the union is how this file
 * avoids retyping a shared header that half the object's constructors take a
 * pointer to; the names stay `unnamed_` because nothing here says what they
 * hold.
 */
#define PARAMS_UNNAMED_06C	(0x06c / 4)	/* float */
#define PARAMS_UNNAMED_070	(0x070 / 4)	/* float */
#define PARAMS_UNNAMED_1B0	(0x1b0 / 4)	/* float */
#define PARAMS_UNNAMED_1B4	(0x1b4 / 4)	/* float */
#define PARAMS_UNNAMED_1B8	(0x1b8 / 4)	/* float */
#define PARAMS_UNNAMED_328	(0x328 / 4)	/* float */

/* `_tagModemParameters::connectionType`; MODEM_CLOCK_DEVIATION's neighbour. */
#define MODEM_CONNECTION_TYPE	(0x048 / 4)

void
V90Demodulator::progress(int *out, unsigned int &nofOut, float *in,
			 unsigned int nofIn)
{
	int *modemParams;
	V90BllState bllState;
	int bllSamples;
	unsigned int prevRate;
	float cur, avg, pdSnr;
	int verdict, whole, frac, whole2, frac2;

	word_38 += nofIn;

	/*
	 * `preFilter.fir.process` until finding F8080: `FloatFIR` is a public
	 * BASE of `V90PreFilter`, not a member, so the FIR's `process` is
	 * inherited and there is no `fir` to qualify it with.  V90PreFilter
	 * declares no `process` of its own, so this is unambiguous.
	 */
	preFilter.process(in, (float *)array_244, nofIn);
	agc.process((const float *)array_244, (float *)array_244, nofIn);
	resampler.resample((const float *)array_244, nofIn,
			   (float *)array_248, word_24c);
	equalizer->process((float *)array_248, word_24c, (short *)array_250,
			   (float *)array_254, word_258);

	word_3c = equalizer->stateCount;

	switch (inPhase3) {
	case 0:
		nofOut = 0;
		break;

	/* ------------------------------------------------ phase 3 ------- */
	case 1:
		nofOut = 0;

		if (spectralVerifier.process(in, nofIn) != 0) {
			switch (spectralVerifier.word_28) {
			case 0:
				autoDigitalImpDetector->setConnectionType(0);
				modemParams =
				    *(int *const *)&V90PB(params)[0];
				modemParams[MODEM_CONNECTION_TYPE] = 0;
				break;

			case 1:
				preFilter.setFilter((unsigned int)
				    params->GERMAN_ISDN_NT1_BOX_FILTER_GAIN);
				edprintf("V90Demodulator: PreFilter adjusted "
					 "to: %d\r\n", preFilter.gain);
				params->LINEAR_EQU_DATA_BETA =
				    params->GERMAN_ISDN_NT1_LINEAR_EQU_DATA_BETA;
				autoDigitalImpDetector->setConnectionType(1);
				modemParams =
				    *(int *const *)&V90PB(params)[0];
				modemParams[MODEM_CONNECTION_TYPE] = 1;
				break;

			case 2:
				preFilter.setFilter((unsigned int)
				    params->GERMAN_PBX_PRE_FILTER_GAIN);
				edprintf("V90Demodulator: PreFilter adjusted "
					 "to: %d\r\n", preFilter.gain);
				params->LINEAR_EQU_DATA_BETA =
				    params->GERMAN_PBX_LINEAR_EQU_DATA_BETA;
				params->DFE_DATA_BETA =
				    params->GERMAN_PBX_DFE_DATA_BETA;
				params->GERMAN_PBX_LINEAR_EQU_DIL_BETA =
				    V90PF(params)[PARAMS_UNNAMED_1B0];
				params->GERMAN_PBX_LINEAR_EQU_DIL_MED_UCODE_BETA =
				    V90PF(params)[PARAMS_UNNAMED_1B4];
				params->GERMAN_PBX_LINEAR_EQU_DIL_HIGH_UCODE_BETA =
				    V90PF(params)[PARAMS_UNNAMED_1B8];
				autoDigitalImpDetector->setConnectionType(2);
				modemParams =
				    *(int *const *)&V90PB(params)[0];
				modemParams[MODEM_CONNECTION_TYPE] = 2;
				break;

			case 3:
				if (preFilter.isV90WithEia6() != 0) {
					edprintf("V90Demodulator: Severe Codec "
						 "conditions were detected due "
						 "to EIA6 loop type...\r\n");
				} else if (params->
				    ENABLE_DROP_2_V34_ON_SEVERE_CODEC != 0) {
					edprintf("V90Demodulator: Severe Codec "
						 "conditions were detected NOT "
						 "on EIA6, initiating drop 2 "
						 "V34...\r\n");
					word_3c = 0x1f;
				} else {
					edprintf("V90Demodulator: Severe Codec "
						 "conditions were detected NOT "
						 "on EIA6, drop is masked, "
						 "doing nothing...\r\n");
				}
				break;

			default:
				edprintf("V90Demodulator: !!! ERROR !!! : "
					 "Spectral Verifier returned illegal "
					 "value !!!\r\n");
				break;
			}
		}

		switch (word_3c) {
		case 0x01:
			if (preFilter.getV90Capability() == 0) {
				word_3c = 0x1f;
				if (DSPLIB_DEBUG_ON())
					dsplibs_debug_printf(
					    "V90Demodulator: Request FallBack "
					    "to V.34 due to line "
					    "conditions\r\n");
			}
			break;

		case 0x03:
			spectralVerifier.startAccumulation();
			if (quickConnect != 0) {
				word_40 = 1;
			} else {
				agc.alpha = params->AGC_K;
				edprintf("V90Demodulator: Agc Activated\r\n");
			}
			break;

		case 0x04:
			if (quickConnect == 0)
				resampler.setBllState(V90_BLL_SLOW, 1);
			break;

		case 0x05:
		case 0x06:
			if (params->PROBING_MODE == 0) {
				if (quickConnect != 0) {
					if (autoDigitalImpDetector->
					    isThereAnyAltRbsPhase() != 0) {
						connectionEvaluator->altRbsDetectedOnQc =
						    1;
						edprintf("V90Demodulator: "
							 "setAltRbsDetectedOnQC "
							 "was called !!!\r\n");
					}
					edprintf("V90Demodulator: "
						 "connectionEvaluator of TRN1d "
						 "is NOT ENABLED due to quick "
						 "connect...\r\n");
				} else {
					connectionEvaluator->word_88 = 1;
					connectionEvaluator->avePdsnrNofSymbols = 0;
					connectionEvaluator->avePdsnr = 0.0f;
					edprintf("V90Demodulator: enabling "
						 "connectionEvaluator of "
						 "TRN1d\r\n");
				}
			}
			break;

		case 0x08:
			if (params->PROBING_MODE == 0) {
				connectionEvaluator->word_84 = 0;
				connectionEvaluator->word_88 = 0;
				connectionEvaluator->avePdsnrNofSymbols = 0;
				connectionEvaluator->avePdsnr = 0.0f;
				edprintf("V90Demodulator: disabling "
					 "connectionEvaluator of Phase3\r\n");
			}
			edprintf("V90Demodulator: Jd maxLookAhead = %d\r\n",
				 sessionFlag != 0 ? jdV92->getMaxLookahead()
						  : jd->getMaxLookahead());
			if (params->PROBING_MODE != 0)
				resampler.setBllState(V90_BLL_FROZEN, 1);
			else
				resampler.setBllState(V90_BLL_DIL, 1);
			break;

		case 0x11:
			if (DSPLIB_DEBUG_ON())
				dsplibs_debug_printf("V90Demodulator: Dil study "
						     "Terminated. Enter error "
						     "relaxation period.\n");
			phase3Demodulator->setDigitalImairmentsInfo();

			trn1dRmsRatio = autoDigitalImpDetector->padGain;
			agc.gain = agc.gain / trn1dRmsRatio;

			whole = (int)agc.gain;
			frac = (int)((agc.gain - (float)whole) * 1.0e6f);
			edprintf("V90Demodulator: Agc Gain = %c%d.%06d\r\n",
				 !(0.0f >= agc.gain) ? '+' : '-',
				 (int)__builtin_fabsf(agc.gain),
				 (frac < 0) ? -frac : frac);

			phase3Demodulator->byte_3f9 = 1;
			connectionEvaluator->word_84 = 0;
			connectionEvaluator->word_88 = 0;
			connectionEvaluator->avePdsnrNofSymbols = 0;
			connectionEvaluator->avePdsnr = 0.0f;
			break;

		case 0x12:
			exitPhase3();
			break;

		case 0x13:
			if (params->PROBING_MODE != 0) {
				if (DSPLIB_DEBUG_ON())
					dsplibs_debug_printf("--------------"
					    "---------------------------------"
					    "----------------\r\n");
				if (DSPLIB_DEBUG_ON())
					dsplibs_debug_printf("V90Demodulator: "
					    "tearing down connection, probing "
					    "mode ended...\r\n");
				if (DSPLIB_DEBUG_ON())
					dsplibs_debug_printf("--------------"
					    "---------------------------------"
					    "----------------\r\n");
				word_3c = 0x2b;
			}
			break;

		case 0x14:
			enterPhase4();
			break;

		case 0x15:
			verdict = connectionEvaluator->indicateLocalRetrain();
			if (verdict == 4)
				word_3c = 0x21;
			else if (verdict == 5)
				word_3c = quickConnect != 0 ? 0x21 : 0x1f;
			break;

		default:
			break;
		}

		/*
		 * THE BLL LADDER.  Six independent tests over one reading of
		 * the resampler's state -- the object loads `bllState` and
		 * `stateSamples` ONCE at 0x1d11b and 0x1d121 and never reloads
		 * them across the four `setBllState` calls, while it DOES
		 * reload `quickConnect` after every one of them (0x1d4ad,
		 * 0x1df92, 0x1dfbb, 0x1dfe4).  That asymmetry is the whole
		 * evidence for the two locals: a call cannot touch a local,
		 * and it can touch an `unsigned int` member that its own
		 * `unsigned int` stores may alias.  `stateSamples` is `int`
		 * here because every comparison against it is a SIGNED `jl`.
		 */
		bllState = resampler.bllState;
		bllSamples = (int)resampler.stateSamples;

		if (quickConnect == 0 && bllState == V90_BLL_INITIAL &&
		    params->BLL_TRN1D_INITIAL_TO_FAST_DURATION < bllSamples)
			resampler.setBllState(V90_BLL_FAST, 1);
		if (quickConnect == 0 && bllState == V90_BLL_FAST &&
		    params->BLL_TRN1D_FAST_TO_SLOW_DURATION < bllSamples)
			resampler.setBllState(V90_BLL_MEDIUM, 1);
		if (quickConnect == 0 && bllState == V90_BLL_SLOW &&
		    params->unnamed_100 < bllSamples)
			resampler.setBllState(V90_BLL_SLOW2, 1);
		if (quickConnect != 0 && bllState == V90_BLL_TRN1_QC_INITIAL &&
		    params->unnamed_104 < bllSamples)
			resampler.setBllState(V90_BLL_TRN1_QC_FAST, 1);
		if (quickConnect != 0 && bllState == V90_BLL_TRN1_QC_FAST &&
		    params->unnamed_108 < bllSamples)
			resampler.setBllState(V90_BLL_TRN1_QC_MEDIUM, 1);
		if (quickConnect != 0 && bllState == V90_BLL_TRN1_QC_MEDIUM &&
		    params->unnamed_10c < bllSamples)
			resampler.setBllState(V90_BLL_TRN1_QC_SLOW, 1);

		/*
		 * THE AGC FREEZE, and the exact-1.0 test is Agc.h's own
		 * statement of what "already frozen" means.
		 */
		if (phase3Demodulator->state == 3 &&
		    params->AGC_ADAPTATION_DURATION <
			(int)phase3Demodulator->word_2c &&
		    agc.alpha != 1.0f) {
			agc.freeze();
			edprintf("V90Demodulator: Agc Frozen\r\n");

			whole = (int)agc.level;
			frac = (int)((agc.level - (float)whole) * 1.0e2f);
			edprintf("V90Demodulator: Agc Energy = %c%d.%02d\r\n",
				 !(0.0f >= agc.level) ? '+' : '-',
				 (int)__builtin_fabsf(agc.level),
				 (frac < 0) ? -frac : frac);

			whole = (int)agc.gain;
			frac = (int)((agc.gain - (float)whole) * 1.0e6f);
			edprintf("V90Demodulator: Agc Gain = %c%d.%06d\r\n",
				 !(0.0f >= agc.gain) ? '+' : '-',
				 (int)__builtin_fabsf(agc.gain),
				 (frac < 0) ? -frac : frac);

			if (quickConnect == 0) {
				if (preFilter.isV90WithEia6() == 0)
					resampler.adjustHalfBaudBpfGain(
					    agc.gain);
			}
			word_40 = 1;

			if (V90PF(params)[PARAMS_UNNAMED_06C] > agc.gain) {
				if (V90PF(params)[PARAMS_UNNAMED_06C] * 0.85 >
				    agc.gain) {
					phase3Demodulator->short_400 = 1;
				} else {
					equalizer->short_08 = (short)
					    ((short)((1.0f - agc.gain /
					      V90PF(params)[PARAMS_UNNAMED_06C])
						     * 250.0f) + 1);
				}
				phase3Demodulator->byte_3f8 = 0x74;

				if (agc.gain <
				    V90PF(params)[PARAMS_UNNAMED_070]) {
					params->unnamed_300 -=
					    params->unnamed_304;
					if (DSPLIB_DEBUG_ON())
						dsplibs_debug_printf(
						    "V90Demodulator: Agc Gain "
						    "very low > Setting DIL "
						    "extreme overflow "
						    "protection !!!\r\n");
				} else if (DSPLIB_DEBUG_ON()) {
					dsplibs_debug_printf(
					    "V90Demodulator: Agc Gain low > "
					    "Setting DIL overflow protection "
					    "!!!\r\n");
				}
			} else {
				phase3Demodulator->byte_3f8 = 0x74;
			}

			edprintf("V90Demodulator: Dil max ucode = %d\n",
				 phase3Demodulator->byte_3f8);
		}

		verdict = connectionEvaluator->evaluatePhase3();
		if (verdict == 4)
			word_3c = 0x21;
		else if (verdict == 5)
			word_3c = quickConnect != 0 ? 0x21 : 0x1f;
		break;

	/* ------------------------------------------------ phase 4 ------- */
	case 2:
		nofOut = 0;

		word_44 += nofIn;
		if (word_44 > word_48) {
			if (DSPLIB_DEBUG_ON())
				dsplibs_debug_printf("V90Demodulator: Phase4 "
						     "TimeOut\r\n");

			if (connectionEvaluator->word_90 != 0 &&
			    phase4Demodulator->int_003c != 0 &&
			    codecType != (__tHardwareCodecTypes__)4) {
				edprintf("V90Demodulator: Silence rrn not "
					 "finished on platform other then USB, "
					 "masking silence rrn...\r\n");
				params->RRN_SILENCE_REQUESTED = 0;
			}
			params->SENSITIVE_ISP_DETECTED = 1;
			params->SILENCE_SCR = 0;
			edprintf("V90Demodulator: On phase4 timeout: Assuming "
				 "Sensitive ISP - use non-silence & 6dB up & "
				 "maixmal rate 12000 for next time...\r\n");

			verdict = connectionEvaluator->indicateLocalRetrain();
			if (verdict == 4)
				word_3c = 0x21;
			else if (verdict == 5)
				word_3c = quickConnect != 0 ? 0x21 : 0x1f;
		}

		switch (word_3c) {
		case 0x19:
			pdSnr = equalizer->calcMeanErrorStatistics();

			if (connectionEvaluator->word_90 != 0 &&
			    phase4Demodulator->int_003c != 0) {
				edprintf("V90Demodulator: Constellation design "
					 "on silence rrn...\r\n");
				additionalCPinfo->word_04 = 1;
				if (sessionFlag != 0)
					phase4Demodulator->enterWaitForCP();
				else
					phase4Demodulator->enterWaitForMP();
				break;
			}

			if (preFilter.isV90WithEia6() != 0)
				pdSnr = 0.0f;

			verdict = connectionEvaluator->
			    evaluateMeanErrorStdPhase4(
				pdSnr, equalizer->meanErrorEnergyMean);
			if (verdict == 4) {
				word_3c = 0x21;
				break;
			}
			if (verdict == 5) {
				word_3c = quickConnect != 0 ? 0x21 : 0x1f;
				break;
			}

			if (quickConnect != 0 &&
			    constellationDesigner->word_24 == 0)
				pdSnr = V90PF(params)[PARAMS_UNNAMED_328] *
					equalizer->meanErrorEnergyMean;
			else
				pdSnr = equalizer->meanErrorEnergyMean;

			verdict = constellationDesigner->process(
			    sessionFlag != 0 ? jdV92->getMaxLookahead()
					     : jd->getMaxLookahead(),
			    autoDigitalImpDetector,
			    pdSnr,
			    sessionFlag != 0 ? jdV92->getRatesMask()
					     : jd->getRatesMask(),
			    mappingParamsAlt,
			    autoDigitalImpDetector->linMapp,
			    autoDigitalImpDetector->linMappAlt,
			    autoDigitalImpDetector->short_2800,
			    autoDigitalImpDetector->byte_280c,
			    phase3Demodulator->getMaxUcode(),
			    (unsigned char)(phase2Info->maxTxPower + 1),
			    codecType,
			    connectionEvaluator->word_94,
			    (V90SpecialSpectralConditions)
				spectralVerifier.word_28);

			connectionEvaluator->enableRrnDown =
			    params->ENABLE_RRN_DOWN;
			connectionEvaluator->enableRrnUp = params->ENABLE_RRN_UP;
			byte_280 = 1;

			if (verdict != 1) {
				edprintf("V90Demodulator: Data Phase spectral "
					 "parameters:\r\n");
				displaySpectralParams(mappingParamsAlt);
				additionalCPinfo->word_04 = 1;
				if (sessionFlag != 0)
					phase4Demodulator->enterWaitForCP();
				else
					phase4Demodulator->enterWaitForMP();
				connectionEvaluator->
				    updateCurrentConstellationData(
					constellationDesigner->short_0a,
					constellationDesigner->float_18,
					constellationDesigner->float_1c,
					constellationDesigner->float_20);
			} else {
				if (DSPLIB_DEBUG_ON())
					dsplibs_debug_printf("V90Demodulator: "
					    "connection design error, "
					    "initiating retrain\r\n");
				if (connectionEvaluator->
				    indicateLocalRetrain() == 5)
					word_3c = quickConnect != 0 ? 0x21
								    : 0x1f;
				else
					word_3c = 0x26;
			}
			break;

		case 0x1a:
		case 0x1b:
			if (phase4Demodulator->state != P4D_STATE_WAIT_FOR_MP)
				break;

			if (params->MASK_RRN_SILENCE_ON_PROBLEMATIC_ISP != 0 &&
			    params->RRN_SILENCE_REQUESTED != 0 &&
			    mp->Trellis == 0 && mp->NonLin == 0 &&
			    mp->Shaping == 0 && mp->Type == 1 &&
			    mp->h1Real == 0 && mp->h1Imag == 0 &&
			    mp->h2Real == 0 && mp->h2Imag == 0 &&
			    mp->h3Real == 0 && mp->h3Imag == 0) {
				edprintf("###########################################################################\r\n");
				if (codecType == (__tHardwareCodecTypes__)4) {
					params->MAX_NOF_V90_RETRAINS = 150;
					edprintf("V90Demodulator: 'Problematic' "
						 "ISP Modem detected on USB. "
						 "Masking drop to V34 "
						 "(MAX_NOF_V90_RETRAINS = "
						 "%d)...\r\n",
						 params->MAX_NOF_V90_RETRAINS);
					connectionEvaluator->word_94 = 1;
				} else {
					params->RRN_SILENCE_REQUESTED = 0;
					edprintf("V90Demodulator: 'Problematic' "
						 "ISP Modem detected NOT on "
						 "USB. Masking Silence "
						 "RRN...\r\n");
				}
				edprintf("###########################################################################\r\n");
			}
			edprintf("V90Demodulator: Silence RRN is not masked "
				 "(silence flag = %d)\r\n",
				 params->RRN_SILENCE_REQUESTED);
			phase4Demodulator->enterWaitForEd();
			break;

		case 0x1c:
			if (connectionEvaluator->word_90 != 0 &&
			    phase4Demodulator->int_003c != 0 &&
			    phase4Demodulator->int_0038 != 0) {
				edprintf("V90Demodulator: freezing timing on "
					 "silence rrn...\r\n");
				resampler.setBllState(V90_BLL_FROZEN, 1);
				word_40 = 0;

				cur = equalizer->meanErrorEnergyCurrent;
				avg = equalizer->meanErrorEnergyMean;
				whole = (int)cur;
				frac = (int)((cur - (float)whole) * 1.0e4f);
				whole2 = (int)avg;
				frac2 = (int)((avg - (float)whole2) * 1.0e4f);
				edprintf("V90Demodulator: Ed received on "
					 "Silence RRN... current Mean Error = "
					 "%c%d.%04d,    average Mean Error = "
					 "%c%d.%04d\r\n",
					 !(0.0f >= cur) ? '+' : '-',
					 (int)__builtin_fabsf(cur),
					 (frac < 0) ? -frac : frac,
					 !(0.0f >= avg) ? '+' : '-',
					 (int)__builtin_fabsf(avg),
					 (frac2 < 0) ? -frac2 : frac2);

				float_274 = equalizer->meanErrorEnergyMean;
			}
			word_270 = 1;
			break;

		case 0x1d:
			enterDataPhase();
			demapper->process((unsigned char *)array_25c, nofOut);
			descrambler.process((const unsigned char *)array_25c,
					    out, nofOut);
			break;

		case 0x28:
			edprintf("V90Demodulator: RtNot detected.\r\n");
			resampler.setBllState(V90_BLL_TRN2, 1);
			word_40 = 1;
			break;

		case 0x2a:
			prevRate = (unsigned int)
			    (mappingParamsAlt->word_0 * 8000u * (1.0f / 6.0f)
			     + 0.5f);
			edprintf("V90Demodulator: on silence RRN redesign, "
				 "prevRate = %d\r\n", prevRate);

			if (connectionEvaluator->word_98 != 0) {
				constellationDesigner->word_48 = 3;
				edprintf("V90Demodulator: FORCED rate down on "
					 "silence rrn\r\n");
			} else if (phase4Demodulator->int_3510 != 0 &&
				   (unsigned int)
				   params->MIN_RATE_FOR_SILENCE_RRN_KEEP_RATE >=
				   prevRate) {
				constellationDesigner->word_48 = 1;
				edprintf("V90Demodulator: keeping rate on "
					 "silence rrn\r\n");
			} else if (params->
			    DEBUG_CONNECTION_EVALUATOR_RATE_DOWN == 2) {
				constellationDesigner->word_48 = 1;
				edprintf("V90Demodulator: FORCED keep rate on "
					 "silence rrn\r\n");
			} else {
				constellationDesigner->word_48 = 3;
				edprintf("V90Demodulator: one rate down on "
					 "silence rrn\r\n");
			}

			cur = equalizer->meanErrorEnergyCurrent;
			avg = equalizer->meanErrorEnergyMean;
			whole = (int)cur;
			frac = (int)((cur - (float)whole) * 1.0e4f);
			whole2 = (int)avg;
			frac2 = (int)((avg - (float)whole2) * 1.0e4f);
			edprintf("V90Demodulator: about to redesign... current "
				 "Mean Error = %c%d.%04d,    average Mean "
				 "Error = %c%d.%04d\r\n",
				 !(0.0f >= cur) ? '+' : '-',
				 (int)__builtin_fabsf(cur),
				 (frac < 0) ? -frac : frac,
				 !(0.0f >= avg) ? '+' : '-',
				 (int)__builtin_fabsf(avg),
				 (frac2 < 0) ? -frac2 : frac2);

			verdict = constellationDesigner->process(
			    sessionFlag != 0 ? jdV92->getMaxLookahead()
					     : jd->getMaxLookahead(),
			    autoDigitalImpDetector,
			    float_274,
			    sessionFlag != 0 ? jdV92->getRatesMask()
					     : jd->getRatesMask(),
			    mappingParamsAlt,
			    autoDigitalImpDetector->linMapp,
			    autoDigitalImpDetector->linMappAlt,
			    autoDigitalImpDetector->short_2800,
			    autoDigitalImpDetector->byte_280c,
			    phase3Demodulator->getMaxUcode(),
			    (unsigned char)(phase2Info->maxTxPower + 1),
			    codecType,
			    connectionEvaluator->word_94,
			    (V90SpecialSpectralConditions)
				spectralVerifier.word_28);

			connectionEvaluator->enableRrnDown =
			    params->ENABLE_RRN_DOWN;
			connectionEvaluator->enableRrnUp = params->ENABLE_RRN_UP;
			byte_280 = 1;

			if (verdict == 1) {
				if (DSPLIB_DEBUG_ON())
					dsplibs_debug_printf("V90Demodulator: "
					    "connection design error, "
					    "initiating retrain\r\n");
				if (connectionEvaluator->
				    indicateLocalRetrain() == 5)
					word_3c = quickConnect != 0 ? 0x21
								    : 0x1f;
				else
					word_3c = 0x26;
			}
			connectionEvaluator->updateCurrentConstellationData(
			    constellationDesigner->short_0a,
			    constellationDesigner->float_18,
			    constellationDesigner->float_1c,
			    constellationDesigner->float_20);
			break;

		case 0x31:
		case 0x33:
			word_270 = 1;
			break;

		case 0x35:
			edprintf("V90Demodulator: freezing timing on silence "
				 "rrn...\r\n");
			resampler.setBllState(V90_BLL_FROZEN, 1);
			word_40 = 0;
			break;

		default:
			break;
		}

		verdict = connectionEvaluator->evaluatePhase4(
		    equalizer->ph4MeanErrorEnergyBeforeToAfterUpdateRatio);
		if (verdict == 4)
			word_3c = 0x21;
		else if (verdict == 5)
			word_3c = quickConnect != 0 ? 0x21 : 0x1f;
		break;

	/* ------------------------------------------------ data ---------- */
	case 3:
		if (word_38 >= (unsigned int)
		    params->MINIMUM_DURATION_IN_DATA_BEFORE_EC_RRN)
			enterDataSteadyState();
		/* FALLTHROUGH -- 0x1cf01 falls into 0x1cf10 */

	case 4:
		demapper->process((unsigned char *)array_25c, nofOut);
		descrambler.process((const unsigned char *)array_25c, out,
				    nofOut);

		if (word_3c == 0x23)
			enterRRN();
		if (word_3c == 0x25)
			enterFPE();

		switch (connectionEvaluator->evaluateConnection()) {
		case 1:
			word_3c = 0x22;
			constellationDesigner->word_48 = 2;
			equalizer->restoreEqualizerToFloat();
			break;
		case 2:
			if (params->DEBUG_CONNECTION_EVALUATOR_RATE_DOWN == 2)
				constellationDesigner->word_48 = 1;
			else
				constellationDesigner->word_48 = 3;
			word_3c = 0x22;
			equalizer->restoreEqualizerToFloat();
			break;
		case 3:
			constellationDesigner->word_48 = 0;
			word_3c = 0x22;
			equalizer->restoreEqualizerToFloat();
			break;
		case 4:
			word_3c = 0x21;
			break;
		case 5:
			word_3c = 0x1f;
			break;
		default:
			break;
		}

		if (sessionFlag != 0 && (word_3c == 0x22 || word_3c == 0x23)) {
			phase4Demodulator->resetBeforRRN();
			phase4Demodulator->int_0040 =
			    connectionEvaluator->word_90;
			demapper->linearMappStudyEnabled = 0;
			if (DSPLIB_DEBUG_ON())
				dsplibs_debug_printf("V90Demodulator: disable "
				    "linear mapping study in data\n");
		}
		break;
	}

	/* ------------------------------------------------ common tail --- */
	if (word_40 != 0) {
		if (agc.level < params->ENERGY_DROP_DETECTOR_THRESHOLD) {
			word_27c += nofIn;
			if (word_27c >= (unsigned int)
			    params->NO_ENERGY_DURATION_FOR_REMOTE_RETRAIN) {
				if (DSPLIB_DEBUG_ON())
					dsplibs_debug_printf("--------------"
					    "---------------------------------"
					    "--------------\r\n");
				if (DSPLIB_DEBUG_ON())
					dsplibs_debug_printf("V90Demodulator: "
					    "REMOTE RETRAIN - Energy Drop "
					    "detected\r\n");
				if (DSPLIB_DEBUG_ON())
					dsplibs_debug_printf("--------------"
					    "---------------------------------"
					    "--------------\r\n");

				if (connectionEvaluator->
				    indicateRemoteRetrain() == 5) {
					if (quickConnect != 0 &&
					    (int)inPhase3 <= 2)
						word_3c = 0x21;
					else
						word_3c = 0x1f;
				} else {
					word_3c = 0x26;
				}
			}
		} else {
			word_27c = 0;
		}
	}

	if (errorEnergyPrintCounter + nofIn < errorEnergyPrintPeriod) {
		errorEnergyPrintCounter += nofIn;
	} else {
		errorEnergyPrintCounter = 0;
		if (DSPLIB_DEBUG_ON()) {
			cur = equalizer->meanErrorEnergyCurrent;
			whole = (int)cur;
			frac = (int)((cur - (float)whole) * 1.0e3f);
			dsplibs_debug_printf("V90Demodulator: Error Energy = "
					     "%c%d.%03d\r\n",
					     !(0.0f >= cur) ? '+' : '-',
					     (int)__builtin_fabsf(cur),
					     (frac < 0) ? -frac : frac);
		}
	}

	if (timingOffsetPrintCounter + nofIn < timingOffsetPrintPeriod) {
		timingOffsetPrintCounter += nofIn;
	} else {
		timingOffsetPrintCounter = 0;
		if (DSPLIB_DEBUG_ON()) {
			frac = (int)((resampler.getTimingOffsetPPM() -
				      (float)(int)resampler.getTimingOffsetPPM())
				     * 1.0e3f);
			dsplibs_debug_printf("V90Demodulator: Timing Offset "
			    "[ppm]  = %c%d.%03d\r\n",
			    !(0.0f >= resampler.getTimingOffsetPPM()) ? '+'
								     : '-',
			    (int)__builtin_fabsf(resampler.getTimingOffsetPPM()),
			    (frac < 0) ? -frac : frac);
		}
	}

	word_260 = (word_260 + word_258) % 6;

	switch (word_3c) {
	case 0x1f:
	case 0x20:
	case 0x21:
		word_268++;
		break;
	case 0x23:
		word_264++;
		break;
	case 0x26:
		word_26c++;
		break;
	default:
		break;
	}
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
 *    sign test is `!(0.0f >= x)`: 0x1c0b4 is `fldz` and 0x1c0dc is `sbb
 *    %eax,%eax; and $0xfffffffe,%eax; add $0x2d,%eax`, the same branchless
 *    `0x2d - 2*CF` the method above uses, so an unordered offset prints '+'.
 *    Being UNGATED is what makes this the cheapest site in the tree to drive
 *    one through -- `off_bits[2]` in `run_reset`.  Findings F2300 and F2410.
 *
 * 3. THE EQUALISER'S CURSOR IS EITHER CONFIGURED OR DERIVED.  A negative
 *    `LINEAR_EQU_CURSOR_PLACE` means "the middle of the linear equaliser",
 *    computed as `linearEquLength >> 1` -- a LOGICAL shift, which is the
 *    second independent statement that the length is unsigned.  The test is
 *    `js`, so zero takes the configured branch.
 *
 * 4. THE LAST STORE IS INTO ANOTHER OBJECT.  The argument goes to this
 *    object's +0x294 and then to the EQUALISER's +0x148, which is the only
 *    write anywhere in the blob to that offset of that class and the reason
 *    `sizeof(V90Equalizer)` is 0x150 (finding F1107).
 */
void
V90Demodulator::reset(unsigned int quickConnectArg)
{
	float offset;
	int whole, frac, cursor;

	if (DSPLIB_DEBUG_ON())
		dsplibs_debug_printf(
		    "V90Demodulator reset, quick connect flag = %d\r\n",
		    quickConnectArg);

	inPhase3 = 0;
	word_27c = 0;
	word_270 = 0;
	word_38 = 0;
	word_3c = 0;
	word_44 = 0;
	word_40 = 0;

	descrambler.reset(0);

	agc.reset();
	agc.blockLen = V90PW(params)[PARAMS_AGC_BLOCK_LEN];
	agc.ref = V90PF(params)[PARAMS_AGC_NOMINAL_ENERGY];

	preFilter.reset();

	offset = V90PF(params)[PARAMS_TIMING_OFFSET];
	whole = (int)offset;
	frac = (int)((offset - (float)whole) * 1000.0f);
	edprintf("V90Demodulator reset: Baud Offset = %c%d.%03d\r\n",
		 !(0.0f >= offset) ? '+' : '-',
		 (int)__builtin_fabsf(offset),
		 (frac < 0) ? -frac : frac);

	v90resampler_reset(&resampler);
	resampler.setTimingOffset(V90PF(params)[PARAMS_TIMING_OFFSET]);

	cursor = V90PW(params)[PARAMS_LINEAR_EQU_CURSOR_PLACE];
	if (cursor < 0)
		equalizer->reset(equalizer->linearEquLength >> 1);
	else
		equalizer->reset((unsigned int)cursor);

	constellationDesigner->reset();

	timingOffsetPrintPeriod = 19200;
	errorEnergyPrintCounter = 0;
	word_24c = 0;
	errorEnergyPrintPeriod = 19200;
	timingOffsetPrintCounter = 0;
	word_258 = 0;
	word_260 = 0;
	word_278 = 0;
	byte_280 = 0;
	quickConnect = quickConnectArg;

	equalizer->quickConnect = quickConnectArg;
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

/*
 * getBitRate -- the downstream rate in bit/s, or zero before there is one.
 *
 * EIGHTY-SEVEN BYTES AND NO CALL, because sixty of them are the x87
 * rounding-mode dance.  What it computes is
 *
 *     0                                        if byte_280 == 0
 *     (unsigned)(mpa->word_0 * 8000 / 6 + 0.5) otherwise
 *
 * and every part of that is read off the instructions rather than inferred:
 *
 *     1b8d9  80 ba 80 02 00 00 00  cmpb   $0x0,0x280(%edx)
 *     1b8e0  74 41                 je     1b923            -> %eax is 0
 *     1b8e2  d9 05 <cst4+0x10c>    flds   0.5f
 *     1b8e8  8b 42 18              mov    0x18(%edx),%eax
 *     1b8ed  69 08 40 1f 00 00     imul   $0x1f40,(%eax),%ecx
 *     1b8fb  d8 0d <cst4+0x108>    fmuls  0.16666667f
 *     1b90a  de c1                 faddp  %st,%st(1)
 *     1b919  df 3c 24              fistpll (%esp)
 *
 * `cmpb $0x0` IS ANY-NON-ZERO, not `> 0`: a byte of 0x80 takes the computing
 * arm, which is what separates this from a `signed char` reading and is in
 * the sweep for that reason.
 *
 * THE TWO CONSTANTS ARE `float`, NOT `double`.  `flds`/`fmuls` are four-byte
 * loads out of `.rodata.cst4`, whose words at +0x108 and +0x10c are
 * 0x3e2aaaab and 0x3f000000 -- the nearest `float` to 1/6, and 0.5 exactly.
 * A source that said `/ 6.0f` would have emitted `fdivs` and one that said
 * `* (1.0 / 6.0)` an eight-byte `fmull`, so the spelling below is what the
 * encoding leaves.
 *
 * THE ARITHMETIC IS UNSIGNED AT BOTH ENDS, and both halves of that are
 * FORCED:
 *
 *   - going in, the product is pushed as a 64-bit pair whose high word was
 *     zeroed at 0x1b8eb, BEFORE the multiply, and read with `fildll`.  A
 *     signed `int` is converted with a bare 32-bit `fildl` and no push;
 *   - coming out, `fistpll` stores SIXTY-FOUR bits and only the low half is
 *     taken.  That is the float-to-`unsigned int` idiom; float-to-`int` is a
 *     32-bit `fistpl`.  Both spellings were put through
 *     `-m32 -O2 -mfpmath=387 -march=i386 -mtune=i686 -fomit-frame-pointer`
 *     to confirm the pair, which is how the return type was settled: the
 *     mangling `_ZNK14V90Demodulator10getBitRateEv` carries no return type
 *     and the header's `void` was a placeholder.
 *
 * The difference is not academic.  `word_0 * 8000` passes 2^31 at word_0 =
 * 268435, and above that the two readings disagree by 2^32/6 -- so the sweep
 * carries values on both sides of it, on finding F613's argument that a
 * signedness the reachable domain never exercises is a defect no test can
 * see.
 *
 * AND THE ADDITION IS NOT DONE AT `float` WIDTH, WHICH IS WHAT THE CAST
 * BELOW IS FOR.  Written `product + 0.5f` this emits ONE instruction,
 * `fadds 0x3f000000`; the object emits TWO, an `flds` of the same four-byte
 * slot hoisted ABOVE the `fildll` and a `faddp %st,%st(1)` after the
 * multiply.  Two operands in x87 registers is what GCC 3.4.2 emits for a
 * commutative operation that is NOT in SFmode -- the SFmode memory form is
 * `*fop_sf_comm` and there is no commutative memory form above it -- so the
 * blob's encoding says the sum was computed wider than the product while the
 * MULTIPLY kept `fmuls`, its own memory operand, and both constants stayed in
 * `.rodata.cst4`.  Widening the whole expression instead gives `fmull` out of
 * `.rodata.cst8` and does not match.
 *
 * V92MODULATOR'S CONSTRUCTOR IS THE INTERNAL CONTROL and it is what makes
 * this a source fact rather than a flag: `blockSize = (unsigned int)(nSamples
 * * ratio + 0.5f)` is the same idiom in the same object, and there the blob
 * emits `fmuls` then `fadds` -- exactly what we emit here.  One object, one
 * compiler, two spellings.
 *
 * SIXTEEN SPELLINGS COMPILED, THREE DISTINCT EMISSIONS, TEN REACH THE OBJECT.
 * The ten are every way of putting the sum above `float`: `(double)` or
 * `(long double)` on the product, on the constant, or on a local holding
 * either.  So the domain is exhausted and the decoded fact is the WIDTH and
 * not this spelling -- `double` and `long double` are indistinguishable here
 * because the value is converted straight to `unsigned int` and never stored.
 * `long double` is written because it is what this tree spells an x87
 * intermediate that never spills (refinement.md lever 12, and
 * V90Equalizer.cpp's file comment).  The six that miss are the baseline, its
 * operand order, a `float` local, and the two whole-expression widenings.
 * Finding F8060.
 */
unsigned int
V90Demodulator::getBitRate() const
{
	if (byte_280 == 0)
		return 0;

	return (unsigned int)((long double)(mappingParamsAlt->word_0 * 8000u
					    * (1.0f / 6.0f)) + 0.5f);
}

/*
 * ===========================================================================
 * THE LIFECYCLE PAIR
 * ===========================================================================
 *
 * `V90Demodulator::V90Demodulator` -- 1002 bytes at 0x1c2b0 (C1) and again at
 * 0x1c6a0 (C2).  `~V90Demodulator` -- 669 bytes at 0x1ad70 (D1) and 0x1b010
 * (D2).  Plain cdecl, `this` as the first STACK argument (finding F215): after
 * `push ebp; push edi; push esi; push ebx; sub $0x4c,%esp` the constructor's
 * fourteen incoming words are at 0x60 through 0x98.
 *
 * FOURTEEN ARGUMENTS, SIX EMBEDDED CONSTRUCTIONS, THIRTEEN ALLOCATIONS AND
 * TWELVE SCALAR STORES.  The six embedded constructions are in the
 * mem-initializer list, in the order the object makes them -- which is
 * DECLARATION order, +0x04c, +0x06c, +0x094, +0x148, +0x1e8, +0x210, so the
 * compiler would put them there whatever the list said.  Writing them out is
 * what gives them their arguments; the two with none, `Agc<float>` and
 * `V90ConstellationPower`, are listed for the record.
 *
 * `Agc<float>` GETS NO DESTRUCTOR AND THAT IS CHECKED, not assumed: `Agc.h`
 * declares `Agc()` and nothing else, so the implicit destructor is trivial and
 * no symbol is emitted for it.  The blob agrees -- the five calls at the end of
 * D2 are the verifier, the descrambler, the constellation power, the resampler
 * and the prefilter, and there is no sixth.
 *
 * FOUR THINGS THE COMPILER WAS FORCED TO ENCODE, and all four are written the
 * object's way rather than the obvious way:
 *
 *  1. `compMode`, ARGUMENT 13, IS NEVER STORED.  `0x94(%esp)` is read exactly
 *     once, at 0x1c982, as the equaliser's eleventh argument.  Argument 14 --
 *     `0x98(%esp)` -- is what lands at +0x30, so the two are not confusable
 *     even though both are word-sized.
 *
 *  2. EVERY LATER USE OF A STORED ARGUMENT RELOADS IT FROM THE MEMBER.
 *     `mov 0x2c(%ebx),...` appears at seven call sites for `params` and
 *     `mov 0x30(%ebx),%ecx` at 0x1c8a9 and 0x1c90a for the session flag, and
 *     `V90Phase4Demodulator`'s eleven arguments come out of +0x14, +0x18,
 *     +0x1e4, +0x24, +0x28, +0x20c, +0x2c, +0x1dc, +0x23c and +0x30 rather
 *     than out of the stack slots they were stored from.  GCC cannot prove
 *     `sysdep_malloc` does not alias `this`, so it must reload what the source
 *     names as a member -- and would NOT reload what the source names as a
 *     parameter.  So the source says `this->`.  The three mem-initializers are
 *     the other way round and for the same reason: they run before any store,
 *     `%edi` is used directly, and they name the PARAMETER.
 *
 *  3. EVERY ALLOCATION KEEPS THE FRESH POINTER IN A LOCAL ACROSS THE
 *     CONSTRUCTOR CALL AND STORES IT TO THE MEMBER AFTERWARDS.  Assigning the
 *     member first and passing the member would make GCC store and then reload
 *     across the call, for the aliasing reason above; V90BitsToSymbol.cpp sets
 *     the argument out in full and V90Modulator.cpp follows it.
 *
 *  4. THE NESTED CONSTRUCTORS ARE CALLED BY THEIR MANGLED NAMES.  The build is
 *     `-nostdinc++`, so there is no <new> and C++ has no other syntax for
 *     running a constructor over storage that already exists; a user-declared
 *     placement form makes GCC emit a null test the blob does not have.  The
 *     original almost certainly wrote `new V90Equalizer(...)` over an inline
 *     `operator new`, and the instruction sequence is the same either way.
 *     The DESTRUCTOR needs no trick: an explicit destructor call is ordinary
 *     C++ and every one of the eight classes is complete here.
 *
 * WHAT THE CONSTRUCTOR DOES NOT WRITE is as much a claim as what it does.
 * +0x034 through +0x048, +0x240, +0x260, +0x270..+0x274 and +0x284..+0x294 are
 * left exactly as the allocator handed them over, so a fresh demodulator's
 * `inPhase3` -- which `enterPhase3` tests against 1 and `sessionTermination`
 * against 3 -- is uninitialised memory.  `V90Modem`'s constructor hands this
 * one a bare `sysdep_malloc(0x298)`, so there is no zeroing anywhere in the
 * chain.  test/unit/t_v90demctor.cpp asserts the seed survives at all fifteen.
 *
 * ---------------------------------------------------------------------------
 * THE STORE ORDER BELOW IS THE OBJECT'S ONLY WHERE THE OBJECT FORCED IT
 *
 * The twelve scalar stores are emitted interleaved with the loads that feed
 * them, and the two zero stores at +0x24c and +0x258 come out in the opposite
 * order to the fields.  That is scheduling, which CLAUDE.md's forced/free rule
 * puts among the things the compiler was free to choose; they are written in
 * the object's sequence where it is legible and not permuted to chase it.
 */

extern "C" {
/*
 * The eight nested constructors, by the names the blob calls at 0x1c863,
 * 0x1c886, 0x1c8be, 0x1c8f3, 0x1c969, 0x1c9ed, 0x1ca18 and 0x1ca48.  C1 is the
 * complete-object variant, which is what a `new` expression uses.
 */
void v90dem_adid_ctor(void *self, V90Parameters *params)
	asm("_ZN25V90AutoDigitalImpDetectorC1EP13V90Parameters");
void v90dem_ce_ctor(void *self, V90Parameters *params)
	asm("_ZN22V90ConnectionEvaluatorC1EP13V90Parameters");
void v90dem_p3d_ctor(void *self, V90Parameters *params,
		     V90SpectralVerifier *verifier, unsigned int flag,
		     V90AutoDigitalImpDetector *adid)
	asm("_ZN20V90Phase3DemodulatorC1EP13V90ParametersP19V90SpectralVerifie"
	    "rjP25V90AutoDigitalImpDetector");
void v90dem_demapper_ctor(void *self, unsigned int levels,
			  V90Parameters *params,
			  V90AutoDigitalImpDetector *adid)
	asm("_ZN11V90DemapperC1EjP13V90ParametersP25V90AutoDigitalImpDetector");
void v90dem_p4d_ctor(void *self, V90MappingParams *mappingParams1,
		     V90MappingParams *mappingParams2, V90Demapper *demapper,
		     V90CP *cp, V90MP *mp,
		     Descrambler<unsigned char, int> *descrambler,
		     V90ConnectionEvaluator *connectionEvaluator,
		     V90Parameters *params,
		     V90Phase3Demodulator *phase3Demodulator,
		     V90AutoDigitalImpDetector *adid, unsigned int flag)
	asm("_ZN20V90Phase4DemodulatorC1EP16V90MappingParamsS1_P11V90DemapperP"
	    "5V90CPP5V90MPP11DescramblerIhiEP22V90ConnectionEvaluatorP13V90Par"
	    "ametersP20V90Phase3DemodulatorP25V90AutoDigitalImpDetectorj");
void v90dem_equ_ctor(void *self, unsigned int linearEquLen,
		     unsigned int dfeLen,
		     V90Phase3Demodulator *phase3Demodulator,
		     V90Phase4Demodulator *phase4Demodulator,
		     V90Demapper *demapper,
		     V90ConnectionEvaluator *connectionEvaluator,
		     V90SpectralVerifier *verifier, V90Parameters *params,
		     V90Resampler *resampler, V90PreFilter *preFilter,
		     V90ComputationalMode mode)
	asm("_ZN12V90EqualizerC1EjjP20V90Phase3DemodulatorP20V90Phase4Demodula"
	    "torP11V90DemapperP22V90ConnectionEvaluatorP19V90SpectralVerifierP"
	    "13V90ParametersP12V90ResamplerP12V90PreFilter20V90ComputationalMo"
	    "de");
void v90dem_trn2_ctor(void *self, V90Parameters *params,
		      V90ConstellationPower *power)
	asm("_ZN15V90TRN2DesignerC1EP13V90ParametersP21V90ConstellationPower");
void v90dem_cd_ctor(void *self, V90Parameters *params, V90PreFilter *preFilter,
		    V90ConstellationPower *power)
	asm("_ZN24V90ConstellationDesignerC1EP13V90ParametersP12V90PreFilterP2"
	    "1V90ConstellationPower");
}

/*
 * The resampler's five immediates.  0x42700000 is 60.0f and 0x3f7ae148 is
 * 0.98f; `V90Resampler`'s own header names the parameters they land in, and
 * the two `float`s pick the `float cutoff` overload rather than the
 * `float *coeffs` one -- which is what the mangling `C1EjfjfP13V90Parametersfj`
 * says was called.
 */
#define V90DEM_RESAMPLER_PHASES		100u
#define V90DEM_RESAMPLER_PPM_SCALE	60.0f
#define V90DEM_RESAMPLER_TAPS		0x10u
#define V90DEM_RESAMPLER_CUTOFF		0.98f
#define V90DEM_RESAMPLER_PPM		0.0f
#define V90DEM_RESAMPLER_MIN_HISTORY	0u

/*
 * The descrambler's three, the same triple `V90Phase3Demodulator` and
 * `V90Modulator` build their own scrambler and descrambler from.
 */
#define V90DEM_DSC_TAP1			0x12u
#define V90DEM_DSC_TAIL			0x17u
#define V90DEM_DSC_OUT			0x63u

/*
 * The five bare heap blocks are `levels` elements of 4, 12, 4, 8 and 8 bytes;
 * the header reads the widths off the address arithmetic in front of each
 * allocation and says why the element TYPES are not derivable from it.  The
 * demapper's first argument is `levels * 2` from the same variable -- the
 * object computes it once, at 0x1c7f4, and uses it for both the twelve-byte
 * width and the demapper.
 */
#define V90DEM_ARRAY_244_WIDTH		4u
#define V90DEM_ARRAY_248_WIDTH		12u
#define V90DEM_ARRAY_250_WIDTH		4u
#define V90DEM_ARRAY_254_WIDTH		8u
#define V90DEM_ARRAY_25C_WIDTH		8u

/*
 * The equaliser's two lengths, which are the only arguments this constructor
 * reads out of the parameter block.  The names are the ORIGINAL AUTHOR'S, out
 * of include/dsplib/V90Parameters.h, which this file cannot include (finding
 * F1112) -- the same convention the three `reset` indices above follow.
 */
#define PARAMS_LINEAR_EQU_LENGTH	(0x170 / 4)	/* int */
#define PARAMS_DFE_LENGTH		(0x1fc / 4)	/* int */

V90Demodulator::V90Demodulator(unsigned int levels, V90Phase2Info *phase2,
			       V90Jd *jdArg, V92Jd *jdV92Arg,
			       tagV90DILdescriptor *dilArg,
			       V90MappingParams *mappingParams1,
			       V90MappingParams *mappingParams2,
			       tagV90AdditionalCPinfo *cpInfo, V90CP *cpArg,
			       V90MP *mpArg, __tHardwareCodecTypes__ codec,
			       V90Parameters *par,
			       V90ComputationalMode compMode, unsigned int flag)
	: agc(),
	  preFilter(codec, phase2, par),
	  resampler(V90DEM_RESAMPLER_PHASES, V90DEM_RESAMPLER_PPM_SCALE,
		    V90DEM_RESAMPLER_TAPS, V90DEM_RESAMPLER_CUTOFF, par,
		    V90DEM_RESAMPLER_PPM, V90DEM_RESAMPLER_MIN_HISTORY),
	  constellationPower(),
	  descrambler(V90DEM_DSC_TAP1, V90DEM_DSC_TAIL, V90DEM_DSC_OUT),
	  spectralVerifier(par)
{
	V90AutoDigitalImpDetector *adid;
	V90ConnectionEvaluator *ce;
	V90Phase3Demodulator *p3d;
	V90Demapper *dem;
	V90Phase4Demodulator *p4d;
	V90Equalizer *equ;
	V90TRN2Designer *trn2;
	V90ConstellationDesigner *cd;

	/*
	 * THE ORDER OF THESE TWELVE IS DECODED, NOT TRANSCRIBED, and the
	 * distinction is refinement.md lever 1's "answer sheet" rule.  The
	 * blob's value-to-slot map is IDENTICAL to ours -- all twelve
	 * arguments reach the same member offsets, measured -- and its STORE
	 * order was what we wrote here, because a store order is the first
	 * thing anybody transcribes off a disassembly.  GCC 3.4.2 permutes
	 * it, so what had to be found is the PREIMAGE of the blob's emission
	 * and that is not readable anywhere.
	 *
	 * Wave 1 compiled every single-element move and every pairwise
	 * transposition of the twelve -- 177 orders, 166 distinct emissions --
	 * and took 77 differing bytes to 23, with the register naming eleven
	 * instructions earlier going right as a bystander.  Wave 2 then
	 * exhausted the order of every statement still misplaced, 5! over the
	 * five heading it and 4! over the four ending it, 2,880 cells and
	 * 2,880 distinct emissions, and reached TWO differing bytes.  Wave 3
	 * is the block below.  Findings F8062 and F8063.
	 */
	phase2Info = phase2;
	jd = jdArg;
	jdV92 = jdV92Arg;
	dil = dilArg;
	mappingParams = mappingParams1;
	params = par;
	mappingParamsAlt = mappingParams2;
	additionalCPinfo = cpInfo;
	sessionFlag = flag;
	cp = cpArg;
	mp = mpArg;
	codecType = codec;

	/*
	 * AND +0x24c IS CLEARED BEFORE +0x258, which is two bytes and the
	 * whole of the residual wave 2 left.  Both stores are zero, so
	 * nothing about the VALUES distinguishes them; what the object
	 * records is which register the allocator gave each, `%esi` to
	 * +0x24c and `%ecx` to +0x258, and that follows the order the two
	 * statements are written in.  Every one of the 42 placements of the
	 * pair among the five allocations was compiled -- the allocations
	 * keep their own order, which is the blob's -- giving 42 distinct
	 * emissions and exactly ONE that reaches the object.  A unique
	 * preimage, so this order is decoded; the other arrangement is 2
	 * bytes and the next nearest is 53.  Finding F8063.
	 */
	array_244 = sysdep_malloc(levels * V90DEM_ARRAY_244_WIDTH);
	array_248 = sysdep_malloc(levels * V90DEM_ARRAY_248_WIDTH);
	array_250 = sysdep_malloc(levels * V90DEM_ARRAY_250_WIDTH);
	array_254 = sysdep_malloc(levels * V90DEM_ARRAY_254_WIDTH);
	array_25c = sysdep_malloc(levels * V90DEM_ARRAY_25C_WIDTH);
	word_24c = 0;
	word_258 = 0;

	adid = (V90AutoDigitalImpDetector *)
	    sysdep_malloc(sizeof(V90AutoDigitalImpDetector));
	v90dem_adid_ctor(adid, params);
	autoDigitalImpDetector = adid;

	ce = (V90ConnectionEvaluator *)
	    sysdep_malloc(sizeof(V90ConnectionEvaluator));
	v90dem_ce_ctor(ce, params);
	connectionEvaluator = ce;

	p3d = (V90Phase3Demodulator *)
	    sysdep_malloc(sizeof(V90Phase3Demodulator));
	v90dem_p3d_ctor(p3d, params, &spectralVerifier, sessionFlag,
			autoDigitalImpDetector);
	phase3Demodulator = p3d;

	dem = (V90Demapper *)sysdep_malloc(sizeof(V90Demapper));
	v90dem_demapper_ctor(dem, levels * 2, params, autoDigitalImpDetector);
	demapper = dem;

	p4d = (V90Phase4Demodulator *)
	    sysdep_malloc(sizeof(V90Phase4Demodulator));
	v90dem_p4d_ctor(p4d, mappingParams, mappingParamsAlt, demapper, cp, mp,
			&descrambler, connectionEvaluator, params,
			phase3Demodulator, autoDigitalImpDetector, sessionFlag);
	phase4Demodulator = p4d;

	equ = (V90Equalizer *)sysdep_malloc(sizeof(V90Equalizer));
	v90dem_equ_ctor(equ, (unsigned int)V90PW(params)[PARAMS_LINEAR_EQU_LENGTH],
			(unsigned int)V90PW(params)[PARAMS_DFE_LENGTH],
			phase3Demodulator, phase4Demodulator, demapper,
			connectionEvaluator, &spectralVerifier, params,
			&resampler, &preFilter, compMode);
	equalizer = equ;

	trn2 = (V90TRN2Designer *)sysdep_malloc(sizeof(V90TRN2Designer));
	v90dem_trn2_ctor(trn2, params, &constellationPower);
	trn2Designer = trn2;

	cd = (V90ConstellationDesigner *)
	    sysdep_malloc(sizeof(V90ConstellationDesigner));
	v90dem_cd_ctor(cd, params, &preFilter, &constellationPower);
	constellationDesigner = cd;

	word_264 = 0;
	word_26c = 0;
	word_268 = 0;
	word_278 = 0;
	byte_280 = 0;
	word_27c = 0;
}

/*
 * `~V90Demodulator` -- an unconditional `sessionTermination()`, then thirteen
 * guarded releases, then the five subobject destructions the compiler emits.
 *
 * THE FIRST INSTRUCTION AFTER THE PROLOGUE IS `call
 * _ZN14V90Demodulator18sessionTerminationEv`, unconditionally, and its 572
 * bytes are not passive: it dereferences `params`, reads the embedded
 * resampler's timing history, prints, and ends by calling
 * `phase3Demodulator->clearVerificationStatus()` with no null test.  So
 * destroying a demodulator whose +0x1dc is null faults inside
 * `sessionTermination`, BEFORE the guard at +0x1dc below can decline to
 * destroy it -- the guard is real code that this object can never reach with a
 * null, and t_v90demctor.cpp records that rather than pretending to test it.
 *
 * The return value is discarded.  `sessionTermination` returns a constant zero
 * whose signedness is want of evidence (see its own comment); no caller in the
 * blob uses it and this one does not either.
 *
 * THE ORDER OF THE THIRTEEN IS THE OBJECT'S and is not the order of the
 * fields: the equaliser first at +0x1d8, then the two phase demodulators and
 * the demapper, then the TRN2 designer from +0x1c, the constellation designer,
 * the impairment detector, the connection evaluator, and last the five bare
 * blocks with no destructor at all.
 *
 * THE NULL TESTS ARE NOT DECORATION.  This tree's `sysdep_free` tolerates
 * NULL, so dropping one leaves every byte comparison unchanged; what moves is
 * `harness_alloc.free_null`, which is what the test asserts.
 *
 * NOTHING IS NULLED AFTER BEING RELEASED, so a second destruction double-frees.
 * That is the blob's behaviour and is reproduced, as it is in
 * V90Modulator.cpp and V90BitsToSymbol.cpp.
 *
 * THE LAST FIVE CALLS ARE NOT WRITTEN HERE and must not be: the compiler emits
 * `~V90SpectralVerifier`, `~Descrambler<unsigned char,int>`,
 * `~V90ConstellationPower`, `~V90Resampler` and `~V90PreFilter` after the body
 * in reverse declaration order, which is exactly the sequence at 0x1b0e3
 * through 0x1b112.  `Agc<float>` has no destructor and gets no call.
 */
V90Demodulator::~V90Demodulator()
{
	sessionTermination();

	if (equalizer) {
		equalizer->~V90Equalizer();
		sysdep_free(equalizer);
	}
	if (phase3Demodulator) {
		phase3Demodulator->~V90Phase3Demodulator();
		sysdep_free(phase3Demodulator);
	}
	if (phase4Demodulator) {
		phase4Demodulator->~V90Phase4Demodulator();
		sysdep_free(phase4Demodulator);
	}
	if (demapper) {
		demapper->~V90Demapper();
		sysdep_free(demapper);
	}
	if (trn2Designer) {
		trn2Designer->~V90TRN2Designer();
		sysdep_free(trn2Designer);
	}
	if (constellationDesigner) {
		constellationDesigner->~V90ConstellationDesigner();
		sysdep_free(constellationDesigner);
	}
	if (autoDigitalImpDetector) {
		autoDigitalImpDetector->~V90AutoDigitalImpDetector();
		sysdep_free(autoDigitalImpDetector);
	}
	if (connectionEvaluator) {
		connectionEvaluator->~V90ConnectionEvaluator();
		sysdep_free(connectionEvaluator);
	}
	if (array_244)
		sysdep_free(array_244);
	if (array_248)
		sysdep_free(array_248);
	if (array_250)
		sysdep_free(array_250);
	if (array_254)
		sysdep_free(array_254);
	if (array_25c)
		sysdep_free(array_25c);
}

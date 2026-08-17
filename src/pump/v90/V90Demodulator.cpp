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
/*
 * The four classes the constructor allocates whose destructors the destructor
 * calls explicitly, and which the header only forward-declares.  Each of the
 * four headers either includes nothing or includes only classes that forward-
 * declare `V90Parameters`, so none of them collides with the block definition
 * V90PreFilter.h supplies through the header above (finding 1112).  The other
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
 * through V90Demodulator.h (finding 1112).
 */
#include "dsplib/TAG_DiagnosticResults.h"
#include "dsplib/tagV90AdditionalCPinfo.h"
/*
 * The header forward-declares this one too, and `getBitRate` DEREFERENCES it.
 * Same argument as V90ConstellationDesigner above.
 */
#include "dsplib/V90MappingParams.h"

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
DEM_OFF(word_284,		0x284, word284);
DEM_OFF(word_278,		0x278, word278);
DEM_OFF(byte_280,		0x280, byte280);
DEM_OFF(word_288,		0x288, word288);
DEM_OFF(word_28c,		0x28c, word28c);
DEM_OFF(word_290,		0x290, word290);
DEM_OFF(word_294,		0x294, word294);

/*
 * `getBitRate` multiplies `mappingParamsAlt`'s FIRST word, and nothing in
 * this tree can mutate that placement -- it is in another file's header --
 * so the assert costs no empirical guarantee (tiers.md's rule about a static
 * check displacing a mutation).
 */
typedef char v90dem_mpar_word0[
    (__builtin_offsetof(V90MappingParams, word_0) == 0) ? 1 : -1];

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
 * file cannot include (finding 1112), and inventing two here would be worse
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
 * Finding 1274; the pairing is why "saved in Registry" is not a figure of
 * speech.
 */
#define MODEM_CLOCK_DEVIATION	(0x04c / 4)

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
 * vptr.  ONE thing rules out the obvious spelling, and it is enough:
 * `resampler.reset()` dispatches through the vptr, which no test fixture here
 * fills in, and would be an indirect call where the object makes a direct one.
 *
 * TWO FURTHER REASONS USED TO BE GIVEN HERE AND ARE NOW FALSE.  Both said the
 * class could not be named at all -- that `V90Resampler.h` brings the OTHER
 * definition of `V90Parameters` (finding 1112) and that declaring the member's
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
 * here (finding 215), and `ResamplerTimingOffset` is `V90Resampler`'s base at
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
	phase3Demodulator->word_410 = word_294;

	equalizer->enterPhase3();

	word_288 = V90PW(params)[PARAMS_WORD_264];
	word_290 = V90PW(params)[PARAMS_WORD_278];

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
 * the scale factors are reciprocal.  Finding 1274.
 *
 * `+0x34` IS A STATE, NOT A LATCH, and this function is what says so.  The
 * diagnostic below prints `isDataState = %d` for `+0x34 == 3`, and
 * `enterChannelVerification` sets the same field to 5; the name `inPhase3` in
 * the header dates from `enterPhase3`, which returns early when it is exactly
 * 1.  The name is left alone -- it is invented either way (finding 226) and
 * eight parallel worktrees share the header -- and corrected here.  Finding
 * 1273.
 *
 * `isV90WithEia6()` IS CALLED TWICE, and the second call is kept because the
 * blob makes it.  The `if` tests it and the `else` prints it, and between the
 * two nothing runs at all -- so caching it would be equivalent and the
 * mutation that does so is uncaught.  Same shape and same reason as
 * `enterPhase3`'s double call, finding 293.  `+0x34 == 3` is likewise
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
 * tells them apart, and the two mutations there die on it.  Findings 2300 and
 * 2410, and `ADID_PRINT_SIGN` in V90AutoDigitalImpDetector.cpp is the same
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

	word_288 = V90PW(params)[PARAMS_WORD_268];
	word_290 = V90PW(params)[PARAMS_WORD_27C];

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

	word_288 = V90PW(params)[PARAMS_WORD_268];
	word_290 = V90PW(params)[PARAMS_WORD_27C];
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
	word_38 = 0;
	word_48 = 0x28230 + 5 * (unsigned int)phase2Info->rtd;

	word_288 = V90PW(params)[PARAMS_WORD_268];
	word_290 = V90PW(params)[PARAMS_WORD_27C];
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
 * and NEITHER line carries a relocation, which is the whole of finding 245's
 * point: they are the integers 12600 and 30000 and not offsets into
 * `.rodata`.  12600 when `word_294` is set and 30000 when it is not, and
 * `word_294` is what `reset` stored `quickConnect` into -- so a quick connect
 * studies for the shorter run.  That reading is the field's provenance and
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

	word_288 = V90PW(params)[PARAMS_WORD_26C];
	word_290 = V90PW(params)[PARAMS_WORD_280];

	resampler.setBllState(V90_BLL_STEADY_STATE, 1);

	if (DSPLIB_DEBUG_ON())
		dsplibs_debug_printf("V90Demodulator: enter Data Phase, "
				     "Rate = %d [bps]\r\n", getBitRate());

	demapper->resetLinearMappStudy(word_294 != 0 ? 12600u : 30000u);
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
 * value therefore prints '+'.  Findings 2300 and 2410.
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
 *    live branches include, which is exactly finding 3511's shape; the cast
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
 *    source has the literal twice; writing it once into a local moves the
 *    code.  Both are `10.0f * log10f(x)`, built as `log10(2) * log2(x)` by
 *    `fldlg2 / fyl2x`, which is how this compiler open-codes `log10f`.
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

	results->word_0bc = 0;
	results->word_0b4 = 8000;

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
 *    one through -- `off_bits[2]` in `run_reset`.  Findings 2300 and 2410.
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
 * carries values on both sides of it, on finding 613's argument that a
 * signedness the reachable domain never exercises is a defect no test can
 * see.
 */
unsigned int
V90Demodulator::getBitRate() const
{
	if (byte_280 == 0)
		return 0;

	return (unsigned int)(mappingParamsAlt->word_0 * 8000u * (1.0f / 6.0f)
			      + 0.5f);
}

/*
 * ===========================================================================
 * THE LIFECYCLE PAIR
 * ===========================================================================
 *
 * `V90Demodulator::V90Demodulator` -- 1002 bytes at 0x1c2b0 (C1) and again at
 * 0x1c6a0 (C2).  `~V90Demodulator` -- 669 bytes at 0x1ad70 (D1) and 0x1b010
 * (D2).  Plain cdecl, `this` as the first STACK argument (finding 215): after
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
 * 1112) -- the same convention the three `reset` indices above follow.
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

	params = par;
	jd = jdArg;
	phase2Info = phase2;
	jdV92 = jdV92Arg;
	mappingParams = mappingParams1;
	dil = dilArg;
	mappingParamsAlt = mappingParams2;
	additionalCPinfo = cpInfo;
	mp = mpArg;
	cp = cpArg;
	codecType = codec;
	sessionFlag = flag;

	array_244 = sysdep_malloc(levels * V90DEM_ARRAY_244_WIDTH);
	array_248 = sysdep_malloc(levels * V90DEM_ARRAY_248_WIDTH);
	array_250 = sysdep_malloc(levels * V90DEM_ARRAY_250_WIDTH);
	array_254 = sysdep_malloc(levels * V90DEM_ARRAY_254_WIDTH);
	array_25c = sysdep_malloc(levels * V90DEM_ARRAY_25C_WIDTH);
	word_258 = 0;
	word_24c = 0;

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

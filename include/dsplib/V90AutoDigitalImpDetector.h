/**
 * @file V90AutoDigitalImpDetector.h
 * @brief `V90AutoDigitalImpDetector`, the V.90 downstream digital-impairment
 *        detector: studies the far end's TRN1 segment to learn each of the
 *        six RBS phases' actual companding behaviour.
 *
 * `V90AutoDigitalImpDetector` is not polymorphic -- its destructor appears
 * with the `D1`/`D2` variants and no `D0`, and GCC emits a deleting
 * destructor only for a virtual one -- so offset 0 is a real member and
 * there is no vptr (finding F228 is the four classes in this tree where
 * that is not true).
 *
 * The object is 43,440 bytes (0xa9b0). The largest `this`-relative
 * displacement any of the class's thirty-two members uses is +0xa9ae, a
 * two-byte access, so the object ends at 0xa9b0, already four-byte aligned
 * (a displacement is not a size, finding F215; the .cpp asserts both the
 * size and every offset below). The bound is the maximum over all
 * thirty-two members, found by disassembling every
 * `_ZN25V90AutoDigitalImpDetector*` symbol and taking the largest
 * displacement in each (finding F251).
 *
 * All thirty-two members are defined and the class is complete: the
 * lifecycle batch wrote `reset` and `resetLinearMapping`; the first
 * processing batch added the sixteen whose only callees were already
 * written; the second added `unitePhasesInfoOfUref` (a leaf) and
 * `updateUref`, which was blocked on exactly that one call; the study batch
 * added the four that read and write the study state at the top of the
 * object; the DIL batch added `updateAltRbsPhaseInDil` with the two members
 * that are its only callers; the pad-gain batch added `determineMaxUcode`
 * and `findPadGain`; and the last batch added `studyUrefHandler`, the
 * per-sample entry point the other thirty-one exist to serve. Every one is
 * differentially tested against the blob by test/unit/t_v90adid.cpp; the
 * intra-class call graph is written down in finding F1367 and closed in
 * finding F1446.
 *
 * The object is mostly six-by-one-hundred-and-twenty-eight arrays: six is
 * the number of RBS phases, and 128 is the seven-bit PCM code magnitude
 * (why `reset` masks its `unsigned char` argument with 0x7f before
 * companding it). The arrays tile the object exactly:
 *
 *     +0x0000  short[6][128]   linMapp        cleared by resetLinearMapping
 *     +0x0600  short[6][128]   linMappAlt     cleared by resetLinearMapping
 *     +0x0d00  uchar[6][128]   set to 1 by reset
 *     +0x1000  float[6][128]   cleared by reset
 *     +0x1c00  uint[6][128]    cleared by reset
 *     +0x2818  short[6][2110]  the received-sample store
 *     +0x8b00  short[6][128]   cleared by reset
 *     +0x9118  float[6][128]   cleared by reset
 *     +0x9d48  float[6][128]   cleared by reset
 *
 * and five per-phase scalars at +0x2800, +0x280c, +0x9100, +0x9d18 and
 * +0x9d30 are cleared alongside them.
 *
 * Three of those types changed once the processing methods were read (and
 * no test could have found the change, since `reset` only ever stores zero
 * into them): +0x1000 is `float`, not `int`; +0x1c00 and +0x9d30 are
 * `unsigned int`, not `int`, each converted to floating point with GCC's
 * unsigned-to-float idiom. The evidence is the disassembly alone -- `make
 * offsets` checks offsets and the differential tests check behaviour, and a
 * field only ever written with zero is invisible to both (finding F1360).
 *
 * What this batch named: +0x0c00 is `prevLinMapp`, 128 shorts, because
 * `setPrevSessionLinearMapping` fills exactly that many and
 * `setQcLinearMapping` reads them back. +0x2818 is `sampleStore`, whose
 * indexing multiplier settles its shape. +0xa956 is `maxUcode[6]`, the six
 * bytes `setMaxUcodeArray` copies in. +0xa9a6 is the `short` threshold
 * `isAltRbs` compares a distance against (finding F1361).
 *
 * Data member names below are invented and mostly offset-derived: the
 * mangling preserves method names and type names but never a data member's
 * name (finding F226). `linMapp` and `linMappAlt` are named for the method
 * whose entire body is clearing them, and `params`, `pcmType`, `ucode` and
 * `ucodeLevel` for what `reset` puts in them. Everything else keeps an
 * offset-derived name or is `pad_`, and every `pad_` region is memory this
 * batch did not model rather than memory known to be unused.
 */

#ifndef DSPLIB_V90AUTODIGITALIMPDETECTOR_H
#define DSPLIB_V90AUTODIGITALIMPDETECTOR_H

/* For `PcmType`.  This header is included, never edited. */
#include "dsplib/V90Phase3Modulator.h"

/*
 * The parameter block the constructor is handed, held at +0x2814.  Not
 * modelled here -- `reset` reads exactly one field of it, the `short` at
 * +0x0c -- and forward-declared rather than included so that this header does
 * not depend on V90PreFilter.h, which declares the same class as a word
 * block.
 */
class V90Parameters;

/* Six RBS phases, and the seven-bit code magnitude they are indexed by. */
#define V90ADID_PHASES	6
#define V90ADID_CODES	128

/*
 * The depth of the per-phase sample store at +0x2818.  It is not a round
 * number and it is not guessed: `addReceivedSampleToStorage` forms its index
 * with `imul $0x83e,%ecx,%esi`, and 6 * 0x83e shorts is 0x62e8 bytes, which
 * is exactly the distance from +0x2818 to the next known field at +0x8b00.
 */
#define V90ADID_SAMPLES	0x83e

class V90AutoDigitalImpDetector {
public:
	/**
	 * @brief Construct a detector, planting its parameter block.
	 *
	 * Plants `params` at +0x2814 and leaves all other 43,425 bytes of the
	 * object exactly as it found them (measured by the test's whole-object
	 * comparison) -- so a freshly constructed detector is unusable until
	 * reset() and resetLinearMapping() have both run. Declared, rather
	 * than left implicit, because the blob carries a real constructor
	 * symbol for it (`C1`/`C2`, fifteen bytes, one store).
	 *
	 * @param params  The session's parameter block. Never dereferenced by
	 *                the constructor itself.
	 */
	V90AutoDigitalImpDetector(V90Parameters *params);

	/**
	 * @brief Destroy a detector. Does nothing.
	 *
	 * Declared (one byte, a bare `ret`) because the blob has the symbol:
	 * an implicit trivial destructor would contribute no `D1`/`D2` at all.
	 */
	~V90AutoDigitalImpDetector();

	/* The two the lifecycle batch defines. */

	/**
	 * @brief Clear the per-phase measurement state for a new session.
	 *
	 * Installs the session's companding law, its reference code and its
	 * "alternate RBS expected" flag. Does not touch `linMapp` or
	 * `linMappAlt`; those belong to resetLinearMapping(), a separate call.
	 *
	 * @param ucode   The reference PCM code the far end will send as TRN1.
	 * @param pcmType mu-law or A-law, this session's companding law.
	 * @param altRbs  Nonzero if the session should expect alternate RBS.
	 */
	void reset(unsigned char ucode, PcmType pcmType, short altRbs);

	/**
	 * @brief Clear both linear-mapping tables and seed the reference code.
	 *
	 * Clears `linMapp` and `linMappAlt`, then writes `ucodeLevel` into
	 * every phase's entry for the reference code `ucode` in both.
	 */
	void resetLinearMapping();

	/*
	 * The eighteen the processing batches define.  Their argument lists are the
	 * mangling's and so are not a guess; a return type is not mangled, so
	 * where one is given below it comes from what the object leaves in
	 * %eax at the `ret` and nothing else.  Three do so deliberately:
	 * `isAltRbs` and `isThereAnyAltRbsPhase` return a 0/1 they compute in
	 * a register zeroed on entry, and `unSuspectedPhaseNearestLinMapp`
	 * returns a `movswl` of a `linMapp` entry -- which is why it is
	 * declared `short` rather than `int`: either spelling emits the same
	 * sign-extending load, so the narrower one carries the extra fact that
	 * the value is a table entry.  The two misspellings are the original
	 * author's.
	 */

	/**
	 * @brief Store one received sample and its histogram bucket.
	 * @param phase   Which of the six RBS phases the sample belongs to.
	 * @param code    The sample's PCM code (used to bump `short_8b00`).
	 * @param sample  The sample's magnitude.
	 */
	void addReceivedSampleToStorage(short phase, unsigned char code,
					float sample);

	/**
	 * @brief Rotate the reference code's `linMapp` column to start at a
	 *        given phase.
	 *
	 * Reads all six entries out before writing any back, so this is a
	 * rotation and not an in-place shift.
	 *
	 * @param offset  The phase to rotate to the front.
	 */
	void adjustUinfoToPhaseOffset(short offset);

	/** @brief Divide both linear-mapping tables through by `padGain`. */
	void applyPadGainToLinMapp();

	/**
	 * @brief Add one sample to a cell's mean/variance accumulators.
	 *
	 * @param magnitude  The sample magnitude to accumulate.
	 * @param level      The linear level whose companded PCM code selects
	 *                    the cell (converted back to a magnitude here, with
	 *                    reset()'s masks run in reverse).
	 * @param phase      Which RBS phase's row to update.
	 */
	void calculateLinearMeanAndVar(short magnitude, short level,
				       unsigned int phase);

	/**
	 * @brief Add one sample to a phase's alternate-RBS accumulators.
	 *
	 * The count is incremented unconditionally, so the eventual mean is
	 * over every sample offered to the phase, not just an accepted subset.
	 *
	 * @param magnitude  The sample magnitude to accumulate.
	 */
	void calculateLinearMeanAndVarAlt(short magnitude, unsigned int);

	/** @brief Clear a phase's alternate-RBS mean/variance accumulators. */
	void clearCamulativeAltVal(short phase, short);

	/** @brief Clear one cell's mean/variance accumulators. */
	void clearCamulativeVal(short phase, short code);

	/**
	 * @brief Does this sample look like alternate RBS on this phase?
	 *
	 * True when the phase is flagged and the sample's magnitude is
	 * further from the phase's established level for this code than the
	 * threshold at +0xa9a6 allows. Both magnitudes are compared as
	 * magnitudes, since the class works in code magnitudes throughout.
	 *
	 * @param phase   The RBS phase to test.
	 * @param code    The PCM code whose established level to compare
	 *                against.
	 * @param sample  The sample's linear level (truncated toward zero).
	 * @return Nonzero if the sample looks like alternate RBS.
	 */
	int isAltRbs(short phase, short code, float sample);

	/** @brief Is any of the six phases flagged as carrying alternate RBS? */
	int isThereAnyAltRbsPhase();

	/**
	 * @brief Install the four thresholds that depend on the connection
	 *        type.
	 * @param type  The parameter block's connection-type value (2 selects
	 *              one set of four thresholds, anything else the other).
	 */
	void setConnectionType(short type);

	/**
	 * @brief Copy in the six per-phase maximum usable codes.
	 * @param from  Six bytes, one per RBS phase.
	 */
	void setMaxUcodeArray(unsigned char *from);

	/**
	 * @brief Copy in the previous session's linear mapping.
	 * @param from  128 shorts, one per PCM code, shared across all six
	 *              phases (see `prevLinMapp`).
	 */
	void setPrevSessionLinearMapping(short *from);

	/**
	 * @brief The entry of `linMapp[phase]` nearest in magnitude to `v`.
	 *
	 * Searches PCM codes 5..116 inclusive only -- the row's two ends are
	 * excluded, which is what "unsuspected" is about: the smallest and
	 * largest codes are the ones a digital impairment reaches first.
	 *
	 * @param v      The value to match.
	 * @param phase  Which phase's `linMapp` row to search.
	 * @return The nearest entry (a table value, hence `short`).
	 */
	short unSuspectedPhaseNearestLinMapp(short v, short phase);

	/**
	 * @brief Group the unflagged phases at one code by how close their
	 *        `linMapp` levels are, and give the flagged phases the level
	 *        and variance of the largest group.
	 * @param code  The PCM code (column of `linMapp`) to unite.
	 */
	void unitePhasesInfoOfUref(short code);

	/**
	 * @brief Turn one cell's accumulators into a mean and a variance.
	 *
	 * Nothing happens when the count is zero, not even a store of zero, so
	 * a cell that never saw a sample keeps whatever resetLinearMapping()
	 * gave it.
	 *
	 * @param phase  Which RBS phase's cell to finish.
	 * @param code   Which PCM code's cell to finish.
	 */
	void updateLinMappMeanAndVar(short phase, short code);

	/** @brief The alternate-RBS counterpart of updateLinMappMeanAndVar(). */
	void updateLinMappMeanAndVarAlt(short phase, short code);

	/**
	 * @brief Turn the reference code's accumulators into a mean and
	 *        variance, unite the phases, then clear the accumulators.
	 *
	 * The three steps run in that order and the clear is unconditional,
	 * so a cell with no samples keeps its mapping entry and every cell
	 * starts the next study empty.
	 */
	void updateUref();

	/**
	 * @brief Round each phase's alternate-RBS mean into `linMappAlt`'s
	 *        reference-code column, then clear the accumulators.
	 *
	 * The clear is unconditional and the update is not: a phase that is
	 * not flagged, or has no samples, still has its accumulators reset.
	 */
	void updateUrefAlt();

	/*
	 * The four the study batch defines.  All four are leaves -- measured
	 * over every `R_386_PC32` in the class, they call only `edprintf` and
	 * `dsplibs_debug_printf` -- and between them they name every byte of
	 * the object that was still `pad_` outside the sample store.
	 *
	 * `getAltVarThresh` returns a `float`, and the mangling does not say
	 * so: the evidence is `flds 0x34(%esp)` immediately before the
	 * epilogue, which leaves a value in %st(0) and nothing in %eax. x87
	 * cannot distinguish a `float` return from a `double` one, so `float`
	 * is the narrower reading and carries the extra fact that a float
	 * local is what comes back.
	 */

	/**
	 * @brief The variance threshold the alternate-RBS test runs against.
	 *
	 * The mean of the below-average half of the six per-phase variances
	 * `var` hands in -- not the mean of all six -- so a single wild phase
	 * is excluded from the threshold it would otherwise set. Scaled by
	 * `factor` and then floored at `altMinVarThresh`.
	 *
	 * @param var     The six per-phase variances to threshold.
	 * @param factor  Scale applied to the below-average mean.
	 * @return The variance threshold.
	 */
	float getAltVarThresh(float *var, float factor);

	/**
	 * @brief The first study pass: turn the middle of each phase's
	 *        accumulators into a mapping and flag the phases too rough
	 *        to trust.
	 *
	 * Covers only PCM codes 0x40..0x4f (the reference code excluded), and
	 * flags a phase as suspected unless more than nine of its fifteen
	 * neighbouring differences in that window are large.
	 */
	void porcessFirstStudy();

	/**
	 * @brief Install the study's distance/variance thresholds and report
	 *        them.
	 *
	 * @param qc  Nonzero selects the QC parameter block's own thresholds
	 *            (scaled from `float_a950`); zero installs the fixed
	 *            defaults with no scaling.
	 */
	void resetStudyUrefHandler(unsigned int qc);

	/**
	 * @brief Group the phases not flagged at `byte_280c` by how close
	 *        their `linMapp` entries are, and give every one of them the
	 *        pooled mean and variance of the largest group.
	 *
	 * Unlike unitePhasesInfoOfUref(), this is a single pass with no
	 * convergence loop, and it clears every phase's accumulators
	 * (flagged phases included) whether or not a group was formed.
	 *
	 * @param at  The PCM code (column of `linMapp`) to unite.
	 */
	void uniteLinMappInfoOfUnsuspectedPhases(unsigned char at);

	/*
	 * The three the DIL batch defines, and they are one cluster on purpose:
	 * `porcessSecondStudy` and `setQcLinearMapping` are the only two
	 * members that call `updateAltRbsPhaseInDil` -- measured over every
	 * `R_386_PC32` in the class -- and beyond that call the three reach
	 * only `memcpy`, `edprintf` and `dsplibs_debug_printf`.
	 *
	 * The two callers share a prologue and an epilogue almost exactly:
	 * each scans `byte_280c` for the first unsuspected phase into
	 * +0xa968, calls `updateAltRbsPhaseInDil`, and then prints the whole
	 * mapping table.  The print loops are NOT the same length -- 0..0x7f
	 * in `porcessSecondStudy` and 0..0x74 in `setQcLinearMapping` -- which
	 * is the one token that separates two otherwise identical blocks.
	 */

	/**
	 * @brief The second study pass: fold suspected phases' recovered
	 *        mapping into the table and report it.
	 *
	 * Finds the first unsuspected phase, calls updateAltRbsPhaseInDil()
	 * to fold the suspected phases' recovered mapping in, then prints the
	 * whole `linMapp` table (codes 0..0x7f).
	 */
	void porcessSecondStudy();

	/**
	 * @brief Build the mapping a QC session starts from.
	 *
	 * Every phase not flagged at `short_2800` gets a mapping: from its own
	 * accumulators if it has a verdict at `byte_280c`, or otherwise from
	 * `prevLinMapp`, the single previous-session row shared by all six
	 * phases. Finds the first unsuspected phase, calls
	 * updateAltRbsPhaseInDil(), and prints the mapping table (codes
	 * 0..0x74).
	 */
	void setQcLinearMapping();

	/**
	 * @brief Rebuild the suspected phases' mapping from the samples they
	 *        actually received, using the unsuspected phase (`unSuspectedPhase`)
	 *        as reference.
	 *
	 * For each suspected phase and each code in a fixed 115-entry scan
	 * order (odd codes 63 down to 3, then 2, then even codes 4 up to 64,
	 * then 65 up to 116), quantises every stored sample for that code to
	 * the nearest entry of the reference phase's mapping, finds the most
	 * popular result among the samples that disagree with the reference,
	 * and gives the phase the reference's entry for `linMapp` and the
	 * popular value for `linMappAlt`.
	 */
	void updateAltRbsPhaseInDil();

	/*
	 * The two the pad-gain batch defines, and they are the two ends of one
	 * story.  Both are LEAVES -- measured over every `R_386_PC32` in the
	 * class, `findPadGain` calls only the four companding routines,
	 * `edprintf` and `dsplibs_debug_printf`, and `determineMaxUcode` calls
	 * only `edprintf`.
	 *
	 * `determineMaxUcode` fills `byte_0d00` and reads it back to give every
	 * phase a `maxUcode`, and it is what leaves `byte_a954` behind.
	 * `findPadGain` starts from that byte, projects the reference phase's
	 * mapping through both companding laws at a range of assumed gains,
	 * takes the gain whose round-trip error is smallest, and stores it in
	 * `padGain` -- which is the only thing that ever makes
	 * `applyPadGainToLinMapp`'s division do anything, because `reset` seeds
	 * the gain with 1.0f.
	 *
	 * Neither returns anything: `determineMaxUcode` ends in a plain `ret`
	 * with %eax holding the last thing it happened to load, and
	 * `findPadGain` ends either in a plain `ret` or in a tail `jmp` to
	 * `dsplibs_debug_printf`.
	 */

	/**
	 * @brief Decide, per phase, which PCM codes are usable and how high
	 *        each phase may be driven.
	 *
	 * Fills `byte_0d00` with a per-cell "usable" mask (usable when the
	 * code is within `maxCode` and the cell's variance is small), leaves
	 * the scan's own top behind in `byte_a954`, and gives every phase a
	 * `maxUcode[]` entry at or below it.
	 *
	 * @param maxCode  The highest code to consider usable at all.
	 */
	void determineMaxUcode(short maxCode);

	/**
	 * @brief Search for the line-pad gain that makes the reference
	 *        phase's mapping round-trip through a companding law
	 *        unchanged, and store it in `padGain`.
	 *
	 * Starts from `byte_a954`, projects the reference phase's mapping
	 * through both companding laws at a range of assumed gains, and
	 * keeps the gain with the smallest round-trip error. Also decides
	 * which companding law the line is actually using and records it in
	 * `int_a960`.
	 */
	void findPadGain();

	/*
	 * The one the last batch defines, and the biggest member of the class:
	 * 5,335 bytes, which is a third again the size of the next largest.
	 * It is the per-sample entry point of the TRN1 study -- one call per
	 * received sample, with the sample's RBS phase as the second argument
	 * -- and it is a state machine on `int_a984` dispatched through a
	 * seven-entry jump table at `.rodata+0xd70`.  The chain the arms name
	 * is NOT in numeric order:
	 *
	 *   0 --a98c--> 1 --a990--> 2 --a994--> 4 --a99c--> 3 --a998--> 5
	 *     --a998--> 6 (terminal)
	 *
	 * IT RETURNS AN int AND THE MANGLING DOES NOT SAY SO.  Every arm leaves
	 * through `mov 0x3c(%esp),%eax` at 0x421a3, off a slot the prologue
	 * seeds with 1.  Three values are reachable: 2 means the study is over,
	 * 0 means "do not use this sample" -- state 1 on an already-flagged
	 * phase, state 2 when the alternate-RBS test fires -- and 1 is
	 * everything else.  Finding F1440.
	 *
	 * It calls five of the class's own members and the object inlines all
	 * five: `calculateLinearMeanAndVar`, `calculateLinearMeanAndVarAlt`,
	 * `isAltRbs`, `updateUref` and `updateUrefAlt`.  The two it really
	 * calls are `getAltVarThresh` and `linear2alaw`/`linear2ulaw` through
	 * the inlined accumulator.
	 *
	 * The constructor `V90AutoDigitalImpDetector(V90Parameters *)` and the
	 * destructor are declared above; they were not, once, because
	 * declaring either makes the class non-trivial and deletes the default
	 * members of a union holding one.  The test fixture uses a byte array
	 * plus a cast for that reason.
	 */

	/**
	 * @brief The per-sample entry point of the TRN1 study.
	 *
	 * Called once per received sample of the far end's TRN1 segment (a
	 * long run of the reference PCM code), with the sample's RBS phase.
	 * Drives a seven-state machine (`int_a984`) through the study's six
	 * timed passes, each ending in an update of `linMapp`/`linMappAlt`
	 * for the reference code, and leaves behind the per-phase
	 * alternate-RBS flags at `short_2800` and `trn1Sigma`.
	 *
	 * @param sample  The received sample's linear level.
	 * @param phase   Which of the six RBS phases the sample belongs to.
	 * @return 2 when the study is complete, 0 when the sample should not
	 *         be used, 1 otherwise.
	 */
	int studyUrefHandler(float sample, unsigned int phase);

	/*
	 * Data members are public because the original's access specifiers are
	 * not recoverable from the mangling (tools/cppstruct.py says so), and
	 * because a single access section is what keeps the class
	 * standard-layout and `__builtin_offsetof` well defined -- the .cpp
	 * asserts every offset below against what the compiler lays out.
	 */

	/*
	 * The two linear-mapping tables, one linear level per RBS phase per
	 * seven-bit code.  `resetLinearMapping` clears both and then writes
	 * `ucodeLevel` into `[phase][ucode]` of each; the "Alt" copy is the
	 * alternate-RBS hypothesis, the same split the class's
	 * `*MeanAndVar` / `*MeanAndVarAlt` and `clearCamulative*` pairs carry
	 * everywhere else.
	 */
	short linMapp[V90ADID_PHASES][V90ADID_CODES];		/* +0x0000 */
	short linMappAlt[V90ADID_PHASES][V90ADID_CODES];	/* +0x0600 */

	/*
	 * One entry per code, not per phase per code -- a single 128-short
	 * row shared by all six phases (finding F1361). `setPrevSessionLinearMapping`
	 * is the writer and `setQcLinearMapping` the reader.
	 */
	short prevLinMapp[V90ADID_CODES];			/* +0x0c00 */

	/*
	 * One flag per phase per code: "this cell's measured mapping is good
	 * enough to use".  `reset` sets all 768 of them to 1 and
	 * `determineMaxUcode` is the only other member that touches it -- which
	 * is what settles the meaning, because that method both fills it and
	 * reads it back in the same call.  It writes 1 into
	 * `[phase][code]` when the code is within the argument's reach AND the
	 * phase's variance for it is small -- `code <= arg` and
	 * `|linearMappingVar[phase][code]| < 2 * varThresh` -- and 0 otherwise,
	 * and then walks the row DOWNWARDS from `byte_a954` looking for the
	 * first 1, which becomes that phase's `maxUcode`.  So a 1 is "usable"
	 * and the array is the per-code mask the per-phase maximum is read out
	 * of.  Finding F1435.
	 *
	 * The reference code's entry is forced back to 1 for every phase at the
	 * end of that fill, whatever the variance said -- see D289.
	 */
	unsigned char byte_0d00[V90ADID_PHASES][V90ADID_CODES];	/* +0x0d00 */

	/*
	 * The per-(phase, code) accumulators the mean and variance are formed
	 * from: the sum of the magnitudes, and the count of samples in it.
	 * `calculateLinearMeanAndVar` adds to the first with `fadds` and
	 * increments the second with `incl`; `updateLinMappMeanAndVar` divides
	 * one by the other.  Both are cleared by `reset` and by
	 * `clearCamulativeVal`.
	 *
	 * The count is UNSIGNED because of how it reaches the FPU: `push $0;
	 * push %eax; fildll` builds a 64-bit value with a zero high word,
	 * which is what GCC emits for `(float)(unsigned int)` and never for a
	 * signed one.
	 */
	float float_1000[V90ADID_PHASES][V90ADID_CODES];	/* +0x1000 */
	unsigned int uint_1c00[V90ADID_PHASES][V90ADID_CODES];	/* +0x1c00 */

	/*
	 * Per-phase.  `short_2800` is compared against 0 by nine members and
	 * is the phase's "suspected" flag; `byte_280c` likewise.  Both are
	 * cleared by `reset` and neither is named here beyond its offset.
	 */
	short short_2800[V90ADID_PHASES];			/* +0x2800 */
	unsigned char byte_280c[V90ADID_PHASES];		/* +0x280c */

	/*
	 * +0x2812..+0x2813 was `pad_2812[2]`: `byte_280c` ends at +0x2812 and
	 * `params` below is a 4-byte-aligned pointer, so natural alignment
	 * inserts exactly these two bytes with the member deleted -- the
	 * existing `ADID_OFF(params, 0x2814, params)` (V90AutoDigitalImpDetector.cpp)
	 * is what proves it. Zero readers/writers anywhere in the object
	 * (`tools/dis.py` over every `V90AutoDigitalImpDetector::` member
	 * function, `0x40200..0x449f0`); removed F10150.
	 */

	/* The constructor's only argument.  `reset` reads its +0x0c. */
	V90Parameters *params;					/* +0x2814 */

	/*
	 * The received-sample store: 2,110 shorts per phase, 25,320 bytes,
	 * and the only field in the object big enough to be interesting on
	 * its own. `addReceivedSampleToStorage` writes
	 * `sampleStore[phase][sampleCount[phase]++]` with no bound on the
	 * index anywhere in the method (docs/deviations.md D256).
	 */
	short sampleStore[V90ADID_PHASES][V90ADID_SAMPLES];	/* +0x2818 */

	/*
	 * A histogram: `addReceivedSampleToStorage` increments
	 * `codeHistogram[phase][code]` once per stored sample, indexed by the
	 * received PCM code rather than by the sample number.
	 */
	short codeHistogram[V90ADID_PHASES][V90ADID_CODES];	/* +0x8b00 */

	/* How many samples phase `p` has in `sampleStore`. */
	int sampleCount[V90ADID_PHASES];			/* +0x9100 */

	/*
	 * Float from every other user -- `fadds`, `fstps`, `fmuls` -- and
	 * cleared by `reset` with a 32-bit zero, which is 0.0f.
	 * `calculateLinearMeanAndVar` accumulates the SQUARE of each magnitude
	 * here where +0x1000 gets the magnitude itself, which is what makes
	 * `updateLinMappMeanAndVar`'s `E[x^2] - E[x]^2` a variance.
	 */
	float float_9118[V90ADID_PHASES][V90ADID_CODES];	/* +0x9118 */

	/*
	 * The alternate-RBS pair of the two above, per phase rather than per
	 * phase per code: `calculateLinearMeanAndVarAlt` adds a magnitude to
	 * one and increments the other, `updateLinMappMeanAndVarAlt` and
	 * `updateUrefAlt` divide, and `clearCamulativeAltVal` and
	 * `updateUrefAlt` clear both.  Unsigned for the same `fildll` reason
	 * as +0x1c00.
	 */
	float altMagnitudeSum[V90ADID_PHASES];			/* +0x9d18 */
	unsigned int altMagnitudeCount[V90ADID_PHASES];	/* +0x9d30 */

	/*
	 * The variance `updateLinMappMeanAndVar` and `updateUref` store.
	 *
	 * The object names it `linearMappingVar`: `determineMaxUcode` prints
	 * exactly this array through "linearMappingVar[%d][%d] = %d\r\n" with
	 * `unSuspectedPhase` and the loop counter as the two indices.  The
	 * identifier is left offset-derived because it is spelled in five files
	 * and a rename buys nothing the comment does not, which is the same
	 * decision finding F1425 records for +0xa9a4 and +0xa9a6.
	 */
	float float_9d48[V90ADID_PHASES][V90ADID_CODES];	/* +0x9d48 */

	/* Cleared by `reset`; `studyUrefHandler` is the only other writer. */
	short short_a948;					/* +0xa948 */

	/*
	 * +0xa94a..+0xa94b was `pad_a94a[2]`: `short_a948` ends at +0xa94a and
	 * `padGain` below is a 4-byte-aligned `float`, so natural alignment
	 * inserts exactly these two bytes with the member deleted -- the
	 * existing `ADID_OFF(padGain, 0xa94c, padgain)`
	 * (V90AutoDigitalImpDetector.cpp) is what proves it. Zero
	 * readers/writers anywhere in the object (same sweep as above);
	 * removed F10150.
	 */

	/*
	 * Set to 1.0f by `reset`.  `findPadGain` stores to it and
	 * `applyPadGainToLinMapp` divides both mapping tables by it, which is
	 * what the two method names say and is now written rather than
	 * predicted -- so the name is no longer offset-derived.
	 */
	float padGain;						/* +0xa94c */

	/*
	 * The divisor every distance threshold is scaled by.
	 * `resetStudyUrefHandler` forms `1.0f / float_a950` once and derives
	 * +0xa9a4, +0xa9a6, +0xa9a8, +0xa9ac and +0xa9ae from it, and it is
	 * the only reader in the class: `flds 0xa950(%ebx)` at 0x408b4.  The
	 * five constants it scales are 25, 50, 4444.4443, 2777.7778 and
	 * 6666.667, and the five values the same method installs when its
	 * argument is zero are 25, 50, 4000, 2500 and 6000 -- so a gain of 1
	 * reproduces three of the five defaults exactly and the field is a
	 * scale rather than an offset.  What sets it is not written yet.
	 */
	float float_a950;					/* +0xa950 */

	/*
	 * The object calls this one `maxUcode` too, and it is the scalar the
	 * six-byte array below is derived from: `determineMaxUcode` prints it
	 * as "original maxUcode = %d" while forcing it up to `short_a97a`, and
	 * then gives every phase a `maxUcode[phase]` at or below it.
	 *
	 * One byte.  `determineMaxUcode` writes it with `mov %bl,0xa954(%ebp)`
	 * at 0x4449f, 0x444d7, 0x4450a and 0x4468a and reads it back with
	 * `movzbl 0xa954(%ebp)`; `findPadGain` reads it three ways --
	 * `movzbl 0xa954(%esi)` at 0x4363e, `cmpb $0x3f,0xa954(%esi)` at
	 * 0x43801 and `cmp %bl,0xa954(%esi)` at 0x438c9 and 0x43986 -- and
	 * every one of the seven accesses is eight bits wide and unsigned.  It
	 * is the top of both of that method's scans.  Finding F1434.
	 *
	 * +0xa955 stays `pad_`: no member of the class names it in any
	 * displacement, so it is memory this batch did not model rather than
	 * memory known to be unused.
	 *
	 * NOT REMOVABLE under the pad-removal workstream (F10150): `maxUcode`
	 * below is `unsigned char[]`, needing only 1-byte alignment, and
	 * +0xa955 is already 1-byte "aligned" by definition -- a field-to-field
	 * gap here would be 0 bytes, not 1, if the member vanished.  The
	 * compiler's own implicit padding does not reproduce this span, same
	 * shape as `VPcmFloModem::pad_6fb8` and `V92CP::pad_40` -- stays
	 * explicit.
	 */
	unsigned char byte_a954;				/* +0xa954 */
	unsigned char pad_a955[1];				/* +0xa955 */

	/*
	 * Six bytes, one per phase, copied in by `setMaxUcodeArray`. The
	 * values are the highest PCM code each phase may be driven at:
	 * `determineMaxUcode` fills them from `byte_a954` above, walking each
	 * phase's `byte_0d00` row down to the first usable code, and clamping
	 * a flagged phase to two below its own argument.
	 */
	unsigned char maxUcode[V90ADID_PHASES];			/* +0xa956 */

	/* The companding law, stored as a full 32-bit copy of the argument. */
	PcmType pcmType;					/* +0xa95c */

	/*
	 * Which companding law `findPadGain` decided the line is using, and the
	 * only thing that writes it: `mov %ecx,0xa960(%esi)` with %ecx zeroed
	 * at 0x43e93, beside the "Final codec identified is MuLaw" print, and
	 * `mov %edi,0xa960(%esi)` with %edi = 1 at 0x440c4, beside "Final codec
	 * identified is ALaw".  Both stores are 32 bits, which is what makes
	 * this an `int` and not a byte, and they retire the whole of what was
	 * `pad_a960[4]`.  Nothing in the class reads it.  Finding F1434.
	 */
	int int_a960;						/* +0xa960 */

	/*
	 * The object names this one itself: `porcessFirstStudy` prints it
	 * through "2.5*trn1Sigma=%d" (finding F1425); `studyUrefHandler` is
	 * the writer. It is the standard deviation the TRN1 segment measured,
	 * and 2.5 of it is the squared-difference threshold that decides
	 * whether a phase's mapping is smooth enough to trust.
	 */
	float trn1Sigma;					/* +0xa964 */

	/*
	 * The object names this one too, as a phase index rather than a
	 * flag: `porcessSecondStudy` prints it as
	 * "V90AutoDigitalImpDetector: unSuspectedPhase = %d\r\n" (finding
	 * F1429). It is the first phase whose `byte_280c` is clear, and the
	 * three readers use it as a `short` row index into `linMapp`.
	 *
	 * The scan cannot reach 6: both writers stop the increment at 5, so
	 * "no unsuspected phase at all" is spelled 5, which is also a valid
	 * phase (docs/deviations.md D285).
	 */
	short unSuspectedPhase;					/* +0xa968 */

	/*
	 * How many of the six phases `porcessFirstStudy` ended up calling
	 * suspected: it stores 0 before the per-phase loop and `incb`s it on
	 * every phase it flags at +0x280c.  A byte, and no other member of
	 * the class touches it.
	 */
	unsigned char byte_a96a;				/* +0xa96a */

	/*
	 * The reference PCM code and the linear level it companded to.  `reset`
	 * takes the code as its first argument, masks it to seven bits,
	 * supplies the sign/company bits the same way V90Phase3Modulator's DIL
	 * expansion does -- `^ 0xd5` for A-law, `^ 0xff` for mu-law -- and
	 * stores the result of the conversion here.
	 */
	unsigned char ucode;					/* +0xa96b */
	short ucodeLevel;					/* +0xa96c */

	/*
	 * `reset`'s third argument, and the only thing it selects: nonzero
	 * puts 5.0f in `float_a970` where zero puts 1.5f.
	 */
	short short_a96e;					/* +0xa96e */

	float float_a970;					/* +0xa970 */
	float float_a974;					/* +0xa974 */

	/*
	 * The four `reset` sets from `params->[+0x0c] == 2`: (1, 88, 0.35f,
	 * 1.75f) when it is 2 and (0, 80, 0.25f, 1.5f) when it is not.
	 */
	short short_a978;					/* +0xa978 */
	short short_a97a;					/* +0xa97a */
	float float_a97c;					/* +0xa97c */
	float float_a980;					/* +0xa980 */

	/*
	 * The study handler's own state, and the six durations it runs
	 * against.  `resetStudyUrefHandler` zeroes the first two and copies
	 * the other six in as 32-bit words from the parameter block -- from
	 * +0x4a8..+0x4bc when its argument is nonzero and from +0x348..+0x35c
	 * when it is zero, six separate `mov` pairs either way.
	 *
	 * A 32-bit `mov` does not name a type, so the copy alone settles
	 * nothing.  `studyUrefHandler` does: it `incl`s +0xa988, loads
	 * +0xa984 with `mov`, and compares +0xa988 against +0xa98c, +0xa990,
	 * +0xa994, +0xa998 and +0xa99c with `cmp`/`je`.  Those are integer
	 * operations on all seven, which rules out `float` and leaves the
	 * signedness open -- every compare seen is an equality.  +0xa9a0 is
	 * copied by `resetStudyUrefHandler` and read by nothing that is
	 * disassembled, so only its width is measured.
	 *
	 * They are eight scalars rather than one array because the object
	 * copies them with eight independent instruction pairs; a loop over
	 * an array would be a loop.
	 */
	int int_a984;						/* +0xa984 */
	int int_a988;						/* +0xa988 */
	int int_a98c;						/* +0xa98c */
	int int_a990;						/* +0xa990 */
	int int_a994;						/* +0xa994 */
	int int_a998;						/* +0xa998 */
	int int_a99c;						/* +0xa99c */
	int int_a9a0;						/* +0xa9a0 */

	/*
	 * The grouping threshold: `unitePhasesInfoOfUref` merges two phases
	 * when their `linMapp` entries for the reference code differ by
	 * STRICTLY LESS than this.  Its neighbour at +0xa9a6 is the same shape
	 * for a single sample against a single entry, and the two are set
	 * together by `resetStudyUrefHandler`.
	 *
	 * The object's own name for this field is `uniteUrefDistanceThresh`.
	 * `resetStudyUrefHandler` prints it with the format string
	 * "uniteUrefDistanceThresh = %d\r\n" and passes exactly
	 * `movswl 0xa9a4(%ebx)` as the argument.  The identifier is left
	 * offset-derived because it is spelled in three files and a rename
	 * buys nothing the comment does not; finding F1425.
	 */
	short short_a9a4;					/* +0xa9a4 */

	/*
	 * The distance threshold `isAltRbs` compares against: it answers yes
	 * when |sample - linMapp[phase][code]| is strictly greater than this.
	 * `resetStudyUrefHandler` is what puts a value here, and prints it
	 * under the object's own name: "altRbsDistanceThresh = %d\r\n" with
	 * `movswl 0xa9a6(%ebx)` as its only argument.  The identifier stays
	 * offset-derived for the reason given at +0xa9a4.
	 */
	short short_a9a6;					/* +0xa9a6 */

	/*
	 * The last eight bytes, all three named by the object's own format
	 * strings (finding F1425): `altMinVarThresh` is the floor
	 * `getAltVarThresh` raises its answer to, and the other two are the
	 * pair `porcessFirstStudy` clamps `2.5 * trn1Sigma` between.
	 * `neighborUcodeMaxDistance`'s displacement, +0xa9ae, is what the
	 * object's 0xa9b0 size is measured from (finding F251).
	 */
	float altMinVarThresh;					/* +0xa9a8 */
	short neighborUcodeMinDistance;				/* +0xa9ac */
	short neighborUcodeMaxDistance;				/* +0xa9ae */
};

#endif /* DSPLIB_V90AUTODIGITALIMPDETECTOR_H */

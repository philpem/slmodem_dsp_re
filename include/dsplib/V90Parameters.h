/*
 * V90Parameters.h -- the parameter block every V.90/V.92/K56Flex object holds
 * a pointer to.
 *
 * Reconstructed from dsplibs.o.  THIS FILE IS A SHARED TYPE AND ITS LAYOUT IS
 * FROZEN: half the constructors in `VPcmV34Main.cpp`'s span take a
 * `V90Parameters *`, so a later batch that changes a field's offset here
 * changes it for every one of them.  Add methods freely; move nothing.
 *
 * THE FIELD NAMES ARE THE ORIGINAL AUTHOR'S, NOT INVENTED.  Finding F226 says
 * the mangling preserves method and type names but never a data member's, and
 * that is still true -- these come from somewhere else.  `loadParams(char *)`
 * is 7,894 bytes of nothing but
 *
 *     Vparser_read_int  (file, "NAME", &this->field)
 *     Vparser_read_float(file, "NAME", &this->field)
 *
 * 295 calls in a straight line, each carrying the parameter's name as an
 * `R_386_32` against `.rodata.str1.1` or `.rodata.str1.4` and its offset as
 * the displacement of a `lea` off `this`.  `tools/vparse.py` reads all 295 --
 * see finding F860 for why that tool needed a clobber rule before its "0
 * unresolved" meant anything.
 *
 * TWO INDEPENDENT MEASUREMENTS AGREE ON THE LAYOUT, which is gates.md's rule
 * 4 and the reason this header is trusted rather than merely plausible:
 *
 *   - `loadParams`   gives 291 distinct (name, offset, int-or-float) triples.
 *   - `setToDefault` gives 339 distinct (offset, width, value) stores, read by
 *     a separate walk of a separate function.
 *
 * They overlap on 289 offsets and disagree on NONE: every offset `loadParams`
 * reads with `Vparser_read_float` gets a default whose bit pattern is a
 * plausible float, and every one it reads with `Vparser_read_int` gets a small
 * integer.  Their union is +0x004..+0x554 in steps of four with no hole, and
 * every store in both is four bytes wide.
 *
 * AND THE SIZE IS MEASURED A THIRD WAY.  `V90Modem`'s constructor does
 * `sysdep_malloc(0x558)` immediately before calling `V90Parameters::
 * V90Parameters`, at .text+0x19551 and again at +0x197b1.  0x554 + 4 == 0x558,
 * so the field span and the allocation agree exactly and there is no trailing
 * padding to argue about.
 *
 * THE FIFTY-ONE `unnamed_*` FIELDS ARE NOT GAPS IN THE MAP.  `setToDefault`
 * writes them and `loadParams` does not, which means exactly what it says:
 * they are parameters the file cannot override.  They are four bytes each and
 * in the right place; only their names are unknown, and inventing one would
 * put a guess where every other line here is a measurement.  Twenty-five of
 * them run consecutively from +0x300 to +0x360 and are very likely one array.
 *
 * NINE OF THE FIFTY-ONE ARE FLOATS AND ARE STILL DECLARED `int`.  Finding F878
 * measured them, and each is annotated below with the value the object stores.
 * Two are FORCED -- an `fsts` writes a `float` and there is no other reading
 * of that instruction -- and the other seven are settled by the bit pattern
 * being an exact round decimal as a float and an arbitrary eight-digit integer
 * as an int, three of them consecutive and in the same numeric band as the
 * named float betas on either side.
 *
 * They are annotated rather than retyped, and that is a deferral with a reason
 * rather than an oversight.  `setToDefault` writes the same four bytes either
 * way, so nothing observable changes; what a retype does change is one
 * mutation in `test/mutations/v90params.json` anchored on the literal
 * `unnamed_1b8 = 0x2d83f0ff;`, and re-anchoring a mutation is how nine of them
 * in another suite came to be measuring a different arm from the one their
 * label named, all nine reported CAUGHT (finding F432).  **A LATER BATCH THAT
 * READS ANY OF THESE NINE MUST READ IT AS A FLOAT**, and the batch that does
 * is the one that should retype it, because it will have a reader to test the
 * change against.
 *
 * `make params` structurally cannot see any of this: `paramcheck.py` compares
 * the header with `loadParams`, and these are exactly the offsets `loadParams`
 * does not read.  The gate is not weaker than it looks -- it answers a
 * different question, over the 291 fields that have a name.
 *
 * TWO FIELDS GO THE OTHER WAY -- +0x4f8 `SENSITIVE_ISP_DETECTED` and +0x4fc
 * `MAX_TX_RATE_INDEX_FOR_SENSITIVE_ISP` are read by `loadParams` and never
 * written by `setToDefault`, so they start as whatever the allocation left.
 *
 * FOUR OFFSETS ARE READ TWICE UNDER TWO NAMES and are marked `alias` below.
 * The parser is called once per name against the same address, so the second
 * read wins if the file carries both; the field is named for the later call
 * and the earlier name is recorded beside it.  `GERMAN_PBX_*` is three of the
 * four, which is the shape you would expect of a per-market override.
 *
 * `Vparser_read_int` and `Vparser_read_float` ARE THREE-BYTE STUBS in the
 * shipped object -- `xor %eax,%eax; ret` -- so none of these reads has any
 * effect at run time and `loadParams` is, behaviourally, a no-op.  It is still
 * the field map, and it is the only thing in the object that knows these
 * names.  An `awk` over `objdump -dr` of the whole of `.text` shows the two
 * `loadParams` members are the ONLY callers of either stub anywhere.
 *
 * Data members are public because the original's access specifiers are not
 * recoverable (finding F226) and because one access section is what keeps
 * `__builtin_offsetof` well defined.
 */

#ifndef DSPLIB_V90PARAMETERS_H
#define DSPLIB_V90PARAMETERS_H

struct _tagModemParameters;

class V90Parameters {
public:
	/*
	 * The members, from the mangling.  All seven are defined in
	 * src/pump/v90/V90Parameters.cpp:
	 *
	 *     loadParams(char *)          7894 B
	 *     setToDefault()              3589 B
	 *     loadModemParamsData()        344 B
	 *     V90Parameters(_tagModemParameters *)   89 B
	 *     init()                        63 B
	 *     initSession()                 24 B
	 *     ~V90Parameters()               1 B   (a bare `ret`)
	 *
	 * `loadParams` USED TO BE LEFT OUT and this comment used to say the
	 * oracle for it did not exist.  It does: `Vparser_read_int` and
	 * `Vparser_read_float` are reconstructed in src/core/Vparser.c, both
	 * sides' copies are weakened out of the differential binary, and a
	 * logging pair records the (reader, name, offset-from-`this`) triple
	 * of every call the blob's member and ours make.  The two 295-entry
	 * sequences are compared in order.  That is not a copy of
	 * `vparse.py`'s output compared with itself: our third argument is
	 * `&this->FIELD` resolved by the COMPILER through this header, so the
	 * offset reaches the log by a path the static walk is not on, and the
	 * ORDER and the PAIRING of the 295 calls are established by nothing
	 * else in the tree.  Findings F879 (which this supersedes) and 6400.
	 */
	V90Parameters(_tagModemParameters *mp);
	~V90Parameters();

	void	setToDefault();
	void	loadParams(char *paramFile);
	void	loadModemParamsData();
	void	init();
	void	initSession();

	/*
	 * +0x000 is a POINTER, not a parameter.  `setToDefault` opens with
	 * `mov 0x0(%ebp),%ebx` and then `mull 0x38(%ebx)`, so it is loaded and
	 * dereferenced rather than assigned a default, and the constructor's
	 * single argument is a `_tagModemParameters *`.
	 */
	_tagModemParameters *modemParams;	/* +0x000 */
	int  	PROBING_MODE;	/* +0x004 */
	int  	HW_CODEC_TYPE;	/* +0x008 */
	int  	LINE_CONNECTION_TYPE;	/* +0x00c */
	int  	ENABLE_EQUALIZER_MMX;	/* +0x010 */
	int  	EIA6_ENABLE_EQUALIZER_MMX;	/* +0x014 */
	int  	PHASE2_INFO_A_OR_MU;	/* +0x018 */
	int  	PHASE2_INFO_RTD;	/* +0x01c */
	int  	PHASE2_INFO_UINFO;	/* +0x020 */
	int  	PHASE2_INFO_MAX_TX_POWER;	/* +0x024 */
	int  	PHASE2_INFO_TX_POWER_MEASURE_POINT;	/* +0x028 */
	int  	DIGITAL_RATE_MASK;	/* +0x02c */
	int  	MAX_SPECTRAL_SHAPER_LOOKAHEAD;	/* +0x030 */
	int  	V34_PHASE4_CONSTELLATION;	/* +0x034 */
	int  	V34_RRN_CONSTELLATION;	/* +0x038 */
	int  	V92_DIGITAL_RATE_MASK;	/* +0x03c */
	int  	V92_MAX_SPECTRAL_SHAPER_LOOKAHEAD;	/* +0x040 */
	float	V92_JD_PHASE;	/* +0x044 */
	int  	ANALOG_RATE_MASK;	/* +0x048 */
	int  	PRE_FILTER_GAIN;	/* +0x04c */
	int  	PRE_FILTER_COEF_TYPE;	/* +0x050 */
	int  	GERMAN_ISDN_NT1_BOX_FILTER_GAIN;	/* +0x054 */
	int  	GERMAN_PBX_PRE_FILTER_GAIN;	/* +0x058 */
	float	AGC_NOMINAL_ENERGY;	/* +0x05c */
	float	AGC_K;	/* +0x060 */
	int  	AGC_BLOCK_LEN;	/* +0x064 */
	int  	AGC_ADAPTATION_DURATION;	/* +0x068 */
	int	unnamed_06c;		/* +0x06c  setToDefault only; the object stores 1.0f (fsts) -- 878 */
	int	unnamed_070;		/* +0x070  setToDefault only; the object stores 0.6f (0x3f19999a) -- 878 */
	/*
	 * +0x074 and +0x078 ARE NAMED BY `V90TRN2Design`'s OWN DIAGNOSTICS,
	 * which is evidence rule 1 -- a format string that prints the thing --
	 * and not usage inference.  Finding F3527 derived both and could not
	 * rename them because another branch held this header; this batch owns
	 * it and applies them.  `loadParams` still does not read either, so the
	 * "setToDefault only" note stands: they are parameters the file cannot
	 * override, and the names come from the printer rather than the parser.
	 *
	 * `maxUcode` -- 0x3cdf1 reads it into the second argument slot of
	 * `"V90TRN2Design: ...hence max ucode (after factor) = %d, by params
	 * maxUcode = %d\r\n"` (.rodata.str1.4+0x9a14), and 0x3ce14 clamps the
	 * computed ucode against the same byte three instructions later, which
	 * is what a ceiling of that name does.
	 *
	 * `nofUcodesInTrn2` -- 0x3d9bc reads it into `"trn2 size  :  %d\r\n"`
	 * (.rodata.str1.1+0x2472).  The string gives the quantity and
	 * `V90TRN2Designer::setNofUcodesInTrn2`'s own MANGLED NAME gives the
	 * spelling, which is the stronger half of the pair: that member's whole
	 * body is `mov 0x80(%edx),%eax ; mov %eax,0x78(%edx)`.
	 * `setTrn2DummyConstel` fills a constellation `while (i < +0x78)` and
	 * `V90TRN2Design` uses it as the count its design loop must reach.
	 */
	int	maxUcode;		/* +0x074  setToDefault only -- 3527 */
	int	nofUcodesInTrn2;	/* +0x078  setToDefault only -- 3527 */
	int	unnamed_07c;		/* +0x07c  setToDefault only */
	/*
	 * +0x080 is where `setNofUcodesInTrn2` COPIES `nofUcodesInTrn2` from
	 * when its `short` argument is non-zero, so it is the configured value
	 * and +0x078 the working one.  No string names it and it keeps its
	 * offset name, exactly as 3527 ruled.
	 */
	int	unnamed_080;		/* +0x080  setToDefault only */
	float	INITIAL_BAUD_OFFSET;	/* +0x084 */
	float	BLL_INITIAL_K1;	/* +0x088 */
	float	BLL_INITIAL_K2;	/* +0x08c */
	float	BLL_FAST_K1;	/* +0x090 */
	float	BLL_FAST_K2;	/* +0x094 */
	float	BLL_MEDIUM_K1;	/* +0x098 */
	float	BLL_MEDIUM_K2;	/* +0x09c */
	float	BLL_SLOW_K1;	/* +0x0a0 */
	float	BLL_SLOW_K2;	/* +0x0a4 */
	float	BLL_SLOW2_K1;	/* +0x0a8 */
	float	BLL_SLOW2_K2;	/* +0x0ac */
	float	BLL_DIL_K1;	/* +0x0b0 */
	float	BLL_DIL_K2;	/* +0x0b4 */
	float	BLL_TRN2_INITIAL_K1;	/* +0x0b8 */
	float	BLL_TRN2_INITIAL_K2;	/* +0x0bc */
	float	BLL_TRN2_K1;	/* +0x0c0 */
	float	BLL_TRN2_K2;	/* +0x0c4 */
	float	BLL_STEADY_STATE_K1;	/* +0x0c8 */
	float	BLL_STEADY_STATE_K2;	/* +0x0cc */
	float	BLL_PRE_ANSPCM_K1;	/* +0x0d0 */
	float	BLL_PRE_ANSPCM_K2;	/* +0x0d4 */
	float	BLL_TRN1_QC_INITIAL_K1;	/* +0x0d8 */
	float	BLL_TRN1_QC_INITIAL_K2;	/* +0x0dc */
	float	BLL_TRN1_QC_FAST_K1;	/* +0x0e0 */
	float	BLL_TRN1_QC_FAST_K2;	/* +0x0e4 */
	float	BLL_TRN1_QC_MEDIUM_K1;	/* +0x0e8 */
	float	BLL_TRN1_QC_MEDIUM_K2;	/* +0x0ec */
	float	BLL_TRN1_QC_SLOW_K2;	/* +0x0f0  alias BLL_TRN1_QC_SLOW_K1 -- D901 */
	int	unnamed_0f4;		/* +0x0f4  setToDefault only; the object stores 2e-12f (0x2c0cbccc) -- 878, and D901 argues this is the real SLOW_K2 */
	int  	BLL_TRN1D_INITIAL_TO_FAST_DURATION;	/* +0x0f8 */
	int  	BLL_TRN1D_FAST_TO_SLOW_DURATION;	/* +0x0fc */
	int	unnamed_100;		/* +0x100  setToDefault only */
	int	unnamed_104;		/* +0x104  setToDefault only */
	int	unnamed_108;		/* +0x108  setToDefault only */
	int	unnamed_10c;		/* +0x10c  setToDefault only */
	float	EIA6_BLL_INITIAL_K1;	/* +0x110 */
	float	EIA6_BLL_INITIAL_K2;	/* +0x114 */
	float	EIA6_BLL_FAST_K1;	/* +0x118 */
	float	EIA6_BLL_FAST_K2;	/* +0x11c */
	float	EIA6_BLL_MEDIUM_K1;	/* +0x120 */
	float	EIA6_BLL_MEDIUM_K2;	/* +0x124 */
	float	EIA6_BLL_SLOW_K1;	/* +0x128 */
	float	EIA6_BLL_SLOW_K2;	/* +0x12c */
	float	EIA6_BLL_SLOW2_K1;	/* +0x130 */
	float	EIA6_BLL_SLOW2_K2;	/* +0x134 */
	float	EIA6_BLL_DIL_K1;	/* +0x138 */
	float	EIA6_BLL_DIL_K2;	/* +0x13c */
	float	EIA6_BLL_TRN2_INITIAL_K1;	/* +0x140 */
	float	EIA6_BLL_TRN2_INITIAL_K2;	/* +0x144 */
	float	EIA6_BLL_TRN2_K1;	/* +0x148 */
	float	EIA6_BLL_TRN2_K2;	/* +0x14c */
	float	EIA6_BLL_STEADY_STATE_K1;	/* +0x150 */
	float	EIA6_BLL_STEADY_STATE_K2;	/* +0x154 */
	int  	EIA6_BLL_TRN1D_INITIAL_TO_FAST_DURATION;	/* +0x158 */
	int  	EIA6_BLL_TRN1D_FAST_TO_SLOW_DURATION;	/* +0x15c */
	int  	TIMING_HISTORY_EVALUATION_ENABLED;	/* +0x160 */
	int  	TIMING_HISTORY_EVALUATION_BUFFER_LENGTH;	/* +0x164 */
	int  	TIMING_HISTORY_EVALUATION_PERIOD;	/* +0x168 */
	float	TIMING_OFFESET_MIN_STD_FOR_SAVE;	/* +0x16c */
	int  	LINEAR_EQU_LENGTH;	/* +0x170 */
	int  	LINEAR_EQU_HISTORY_LENGTH;	/* +0x174 */
	int  	LINEAR_EQU_FADE_EDGES_CYCLE;	/* +0x178 */
	float	LINEAR_EQU_FADE_LEFT_EDGE_RATIO;	/* +0x17c */
	float	LINEAR_EQU_FADE_RIGHT_EDGE_RATIO;	/* +0x180 */
	int  	LINEAR_EQU_CURSOR_PLACE;	/* +0x184 */
	float	LINEAR_EQU_TRN1D_BETA;	/* +0x188 */
	float	GERMAN_PBX_LINEAR_EQU_DIL_BETA;	/* +0x18c  alias LINEAR_EQU_DIL_BETA */
	float	GERMAN_PBX_LINEAR_EQU_DIL_MED_UCODE_BETA;	/* +0x190  alias LINEAR_EQU_DIL_MED_UCODE_BETA */
	float	GERMAN_PBX_LINEAR_EQU_DIL_HIGH_UCODE_BETA;	/* +0x194  alias LINEAR_EQU_DIL_HIGH_UCODE_BETA */
	float	EIA6_LINEAR_EQU_DIL_MED_UCODE_BETA;	/* +0x198 */
	float	EIA6_LINEAR_EQU_DIL_HIGH_UCODE_BETA;	/* +0x19c */
	float	EIA6_LINEAR_EQU_DIL_ERROR_RELAX_BETA;	/* +0x1a0 */
	float	LINEAR_EQU_ALT_DIL_BETA;	/* +0x1a4 */
	float	LINEAR_EQU_ALT_DIL_MED_UCODE_BETA;	/* +0x1a8 */
	float	LINEAR_EQU_ALT_DIL_HIGH_UCODE_BETA;	/* +0x1ac */
	int	unnamed_1b0;		/* +0x1b0  setToDefault only; the object stores 8.5e-11f (0x2ebaeabf) -- 878 */
	int	unnamed_1b4;		/* +0x1b4  setToDefault only; the object stores 6e-11f (0x2e83f0ff) -- 878 */
	int	unnamed_1b8;		/* +0x1b8  setToDefault only; the object stores 1.5e-11f (fsts) -- 878 */
	float	LINEAR_EQU_DIL_ERROR_RELAX_BETA;	/* +0x1bc */
	float	LINEAR_EQU_TRN2D_INITIAL_BETA;	/* +0x1c0 */
	float	LINEAR_EQU_TRN2D_BETA;	/* +0x1c4 */
	float	LINEAR_EQU_DATA_BETA;	/* +0x1c8 */
	int  	LINEAR_EQU_TRN1D_FREEZE_DURATION;	/* +0x1cc */
	int  	LINEAR_EQU_TRN2D_INITIAL_DURATION;	/* +0x1d0 */
	float	EIA6_LINEAR_EQU_TRN1D_BETA;	/* +0x1d4 */
	float	EIA6_LINEAR_EQU_DIL_BETA;	/* +0x1d8 */
	float	EIA6_LINEAR_EQU_TRN2D_BETA;	/* +0x1dc */
	float	EIA6_LINEAR_EQU_DATA_BETA;	/* +0x1e0 */
	float	EIA6_LINEAR_EQU_TRN2D_INITIAL_BETA;	/* +0x1e4 */
	int  	EIA6_LINEAR_EQU_FADE_EDGES_CYCLE;	/* +0x1e8 */
	float	EIA6_LINEAR_EQU_FADE_LEFT_EDGE_RATIO;	/* +0x1ec */
	float	EIA6_LINEAR_EQU_FADE_RIGHT_EDGE_RATIO;	/* +0x1f0 */
	float	GERMAN_PBX_LINEAR_EQU_DATA_BETA;	/* +0x1f4 */
	float	GERMAN_ISDN_NT1_LINEAR_EQU_DATA_BETA;	/* +0x1f8 */
	int  	DFE_LENGTH;	/* +0x1fc */
	float	DFE_TRN1D_BETA;	/* +0x200 */
	float	DFE_DIL_BETA;	/* +0x204 */
	float	DFE_TRN2D_BETA;	/* +0x208 */
	float	DFE_DATA_BETA;	/* +0x20c */
	float	DFE_DIL_MED_UCODE_BETA;	/* +0x210 */
	float	DFE_DIL_HIGH_UCODE_BETA;	/* +0x214 */
	float	EIA6_DFE_DIL_MED_UCODE_BETA;	/* +0x218 */
	float	EIA6_DFE_DIL_HIGH_UCODE_BETA;	/* +0x21c */
	float	EIA6_DFE_DIL_ERROR_RELAX_BETA;	/* +0x220 */
	float	DFE_DIL_ALT_BETA;	/* +0x224 */
	float	DFE_DIL_ALT_MED_UCODE_BETA;	/* +0x228 */
	float	DFE_DIL_ALT_HIGH_UCODE_BETA;	/* +0x22c */
	float	DFE_DIL_ERROR_RELAX_BETA;	/* +0x230 */
	int  	DFE_TRN1D_FREEZE_DURATION;	/* +0x234 */
	float	GERMAN_PBX_DFE_TRN2D_FAST_BETA;	/* +0x238 */
	float	GERMAN_PBX_DFE_TRN2D_SLOW_BETA;	/* +0x23c */
	float	GERMAN_PBX_DFE_DATA_BETA;	/* +0x240 */
	float	EIA6_DFE_DIL_BETA;	/* +0x244 */
	float	EIA6_DFE_TRN1D_BETA;	/* +0x248 */
	float	EIA6_DFE_DATA_BETA;	/* +0x24c */
	float	EIA6_DFE_TRN2D_FAST_BETA;	/* +0x250 */
	float	EIA6_DFE_TRN2D_SLOW_BETA;	/* +0x254 */
	float	EIA6_DFE_TRN2D_RRN_BETA;	/* +0x258 */
	int  	ERROR_ENERGY_MEAN_BLOCK_LEN;	/* +0x25c */
	float	ERROR_ENERGY_MEAN_K;	/* +0x260 */
	int  	ERROR_ENERGY_PRINT_PERIOD_PHASE3;	/* +0x264 */
	int  	ERROR_ENERGY_PRINT_PERIOD_PHASE4;	/* +0x268 */
	int  	ERROR_ENERGY_PRINT_PERIOD_DATA;	/* +0x26c */
	int  	NOF_DD_SYMBOLS_BEFORE_MEAN_ERROR_DIAG_PHASE3;	/* +0x270 */
	int  	NOF_DD_SYMBOLS_BEFORE_MEAN_ERROR_DIAG_PHASE4;	/* +0x274 */
	int  	TIMING_OFFSET_PRINT_PERIOD_PHASE3;	/* +0x278 */
	int  	TIMING_OFFSET_PRINT_PERIOD_PHASE4;	/* +0x27c */
	int  	TIMING_OFFSET_PRINT_PERIOD_DATA;	/* +0x280 */
	float	SD_DETECTOR_ENERGY_THRESHOLD;	/* +0x284 */
	float	SD_DETECTOR_POSITIVE_CORR_THRESHOLD;	/* +0x288 */
	float	SD_DETECTOR_NEGATIVE_CORR_THRESHOLD;	/* +0x28c */
	int  	SD_DETECTOR_DETECTION_COUNTER_THRESHOLD;	/* +0x290 */
	int  	PHASE4_R_DETECTION_LENGTH;	/* +0x294 */
	int  	RRN_R_DETECTION_LENGTH;	/* +0x298 */
	float	ENERGY_DROP_DETECTOR_THRESHOLD;	/* +0x29c */
	int  	NO_ENERGY_DURATION_FOR_REMOTE_RETRAIN;	/* +0x2a0 */
	int  	SPECTRAL_VERIFIER_ENABLE;	/* +0x2a4 */
	int  	EIA6_SPECTRAL_VERIFIER_ENABLE;	/* +0x2a8 */
	float	SPECTRAL_VERIFIER_SAMPLE_FREQ;	/* +0x2ac */
	int  	SPECTRAL_VERIFIER_FFT_LEN;	/* +0x2b0 */
	int  	SPECTRAL_VERIFIER_FFT_WINDOW;	/* +0x2b4 */
	int  	SPECTRAL_VERIFIER_PSD_LEN;	/* +0x2b8 */
	int  	SPECTRAL_VERIFIER_PSD_OVERLAP_LEN;	/* +0x2bc */
	int  	SPECTRAL_VERIFIER_PRINT_SPECTRUM;	/* +0x2c0 */
	float	SPECTRAL_VERIFIER_ISDN_NULL_FREQ;	/* +0x2c4 */
	float	SPECTRAL_VERIFIER_ISDN_LEFT_PEAK_FREQ;	/* +0x2c8 */
	float	SPECTRAL_VERIFIER_ISDN_RIGHT_PEAK_FREQ;	/* +0x2cc */
	float	SPECTRAL_VERIFIER_ISDN_LEFT_PEAK_DELTA;	/* +0x2d0 */
	float	SPECTRAL_VERIFIER_ISDN_RIGHT_PEAK_DELTA;	/* +0x2d4 */
	float	SPECTRAL_VERIFIER_GERMAN_PBX_NULL_FREQ;	/* +0x2d8 */
	float	SPECTRAL_VERIFIER_GERMAN_PBX_LEFT_PEAK_FREQ;	/* +0x2dc */
	float	SPECTRAL_VERIFIER_GERMAN_PBX_RIGHT_PEAK_FREQ;	/* +0x2e0 */
	float	SPECTRAL_VERIFIER_GERMAN_PBX_LEFT_PEAK_DELTA;	/* +0x2e4 */
	float	SPECTRAL_VERIFIER_GERMAN_PBX_RIGHT_PEAK_DELTA;	/* +0x2e8 */
	float	SPECTRAL_VERIFIER_SEVERE_CODEC_REF_FREQ;	/* +0x2ec */
	float	SPECTRAL_VERIFIER_SEVERE_CODEC_TEST_FREQ1;	/* +0x2f0 */
	float	SPECTRAL_VERIFIER_SEVERE_CODEC_TEST_FREQ2;	/* +0x2f4 */
	float	SPECTRAL_VERIFIER_SEVERE_CODEC_DELTA;	/* +0x2f8 */
	int  	TRN1D_DD_LENGTH;	/* +0x2fc */
	int	unnamed_300;		/* +0x300  setToDefault only */
	int	unnamed_304;		/* +0x304  setToDefault only */
	int	unnamed_308;		/* +0x308  setToDefault only */
	int	unnamed_30c;		/* +0x30c  setToDefault only */
	int	unnamed_310;		/* +0x310  setToDefault only */
	int	unnamed_314;		/* +0x314  setToDefault only */
	int	unnamed_318;		/* +0x318  setToDefault only */
	int	unnamed_31c;		/* +0x31c  setToDefault only */
	int	unnamed_320;		/* +0x320  setToDefault only */
	int	unnamed_324;		/* +0x324  setToDefault only */
	int	unnamed_328;		/* +0x328  setToDefault only; the object stores 0.96f (0x3f75c28f) -- 878 */
	int	unnamed_32c;		/* +0x32c  setToDefault only */
	int	unnamed_330;		/* +0x330  setToDefault only */
	int	unnamed_334;		/* +0x334  setToDefault only */
	int	unnamed_338;		/* +0x338  setToDefault only */
	int	unnamed_33c;		/* +0x33c  setToDefault only */
	int	unnamed_340;		/* +0x340  setToDefault only */
	int	unnamed_344;		/* +0x344  setToDefault only */
	int	unnamed_348;		/* +0x348  setToDefault only */
	int	unnamed_34c;		/* +0x34c  setToDefault only */
	int	unnamed_350;		/* +0x350  setToDefault only */
	int	unnamed_354;		/* +0x354  setToDefault only */
	int	unnamed_358;		/* +0x358  setToDefault only */
	int	unnamed_35c;		/* +0x35c  setToDefault only */
	int	unnamed_360;		/* +0x360  setToDefault only */
	int  	SILENCE_SCR;	/* +0x364 */
	int  	MINIMUM_RTD_FOR_NON_SILENCE_SCR;	/* +0x368 */
	int  	TRN2D_DD_LENGTH;	/* +0x36c */
	int  	RRN_TRN2D_DD_LENGTH;	/* +0x370 */
	int  	USE_RESTRICED_DMIN;	/* +0x374 */
	int  	ENABLE_REDUNDANCY_OPTIMIZATION;	/* +0x378 */
	int  	ENABLE_DIGITAL_POWER_REDUCTION;	/* +0x37c */
	float	DIGITAL_POWER_REDUCTION;	/* +0x380 */
	float	UP_ROUND_K;	/* +0x384 */
	int  	EIA6_USE_RESTRICED_DMIN;	/* +0x388 */
	float	DMIN_CALC_FACTOR1;	/* +0x38c */
	int  	DMIN_CALC_FACTOR2;	/* +0x390 */
	float	DMIN_EIA6_FACTOR;	/* +0x394 */
	int  	FORCED_DMIN;	/* +0x398 */
	int	unnamed_39c;		/* +0x39c  setToDefault only */
	int  	FORCE_RATE_ENABLE;	/* +0x3a0 */
	int  	RATE_FORCE;	/* +0x3a4 */
	float	SPECTRAL_SHAPER_A1;	/* +0x3a8 */
	float	SPECTRAL_SHAPER_A2;	/* +0x3ac */
	float	SPECTRAL_SHAPER_B1;	/* +0x3b0 */
	float	SPECTRAL_SHAPER_B2;	/* +0x3b4 */
	int  	SPECTRAL_SHAPER_SR;	/* +0x3b8 */
	int  	SPECTRAL_SHAPER_ID;	/* +0x3bc */
	float	GERMAN_PBX_SPECTRAL_SHAPER_A1;	/* +0x3c0 */
	float	GERMAN_PBX_SPECTRAL_SHAPER_A2;	/* +0x3c4 */
	float	GERMAN_PBX_SPECTRAL_SHAPER_B1;	/* +0x3c8 */
	float	GERMAN_PBX_SPECTRAL_SHAPER_B2;	/* +0x3cc */
	int  	GERMAN_PBX_SPECTRAL_SHAPER_SR;	/* +0x3d0 */
	int  	GERMAN_PBX_SPECTRAL_SHAPER_ID;	/* +0x3d4 */
	float	EIA6_SPECTRAL_SHAPER_A1;	/* +0x3d8 */
	float	EIA6_SPECTRAL_SHAPER_A2;	/* +0x3dc */
	float	EIA6_SPECTRAL_SHAPER_B1;	/* +0x3e0 */
	float	EIA6_SPECTRAL_SHAPER_B2;	/* +0x3e4 */
	int  	EIA6_SPECTRAL_SHAPER_SR;	/* +0x3e8 */
	int  	EIA6_SPECTRAL_SHAPER_ID;	/* +0x3ec */
	int  	RRN_SILENCE_REQUESTED;	/* +0x3f0 */
	int  	MASK_RRN_SILENCE_ON_PROBLEMATIC_ISP;	/* +0x3f4 */
	int  	RRN_SILENCE_SCR_LENGTH;	/* +0x3f8 */
	int  	RRN_SILENCE_WAIT_BEFORE_ECHO_CALC;	/* +0x3fc */
	int  	RRN_SILENCE_ECHO_CALC_PERIOD;	/* +0x400 */
	float	RRN_SILENCE_MIN_ECHO_ENERGY_FOR_KEEP_RATE;	/* +0x404 */
	int  	MIN_RATE_FOR_SILENCE_RRN_KEEP_RATE;	/* +0x408 */
	float	PDSNR_THRESHOLD_IN_PHASE3;	/* +0x40c */
	float	PDSNR_THRESHOLD_IN_PHASE4;	/* +0x410 */
	float	TRN1D_ERROR_FOR_V34_FALLBACK;	/* +0x414 */
	int  	TRN1D_MEAN_ERROR_STD_EVALUATION_ENABLE;	/* +0x418 */
	float	TRN1D_MAX_MEAN_ERROR_STD_IN_PHASE3;	/* +0x41c */
	int  	TRN2D_MEAN_ERROR_STD_EVALUATION_ENABLE;	/* +0x420 */
	float	TRN2D_MAX_MEAN_ERROR_STD_IN_PHASE4;	/* +0x424 */
	float	TRN2D_MAX_MEAN_ERROR_ENERGY_IN_PHASE4;	/* +0x428 */
	float	PHASE3_ERROR_FOR_V34_FALLBACK;	/* +0x42c */
	float	PHASE4_ERROR_FOR_V34_FALLBACK;	/* +0x430 */
	int	unnamed_434;		/* +0x434  setToDefault only; the object stores 250.0f (0x437a0000) -- 878 */
	float	PHASE4_MEAN_ERROR_BEF_TO_AFT_UPDATE_RATIO_THRESH;	/* +0x438 */
	float	QC_PHASE4_MEAN_ERROR_BEF_TO_AFT_UPDATE_RATIO_THRESH;	/* +0x43c */
	/*
	 * A FLOAT, and it was typed `int` here only because 878 could see
	 * the bit pattern and not the use.  `V90Phase3Demodulator::getV90Decision`
	 * copies this word into +0x438 above -- a `float` -- with a plain
	 * `mov`, which is what GCC emits for a float-to-float assignment and
	 * never for an int-to-float one.  `setToDefault` stores the same
	 * four bytes either way.
	 */
	float	unnamed_440;		/* +0x440  setToDefault only; 10.0f -- 878 */
	int  	ENABLE_RRN_UP;	/* +0x444 */
	int  	ENABLE_RRN_DOWN;	/* +0x448 */
	int  	RATE_UP_DETECT_DURATION;	/* +0x44c */
	int  	RATE_DOWN_DETECT_DURATION;	/* +0x450 */
	int  	RETRAIN_DETECT_DURATION;	/* +0x454 */
	int  	NOF_REMOTE_RATE_RENEG_BEFORE_RETRAIN;	/* +0x458 */
	int	unnamed_45c;		/* +0x45c  setToDefault only */
	int  	MAX_NOF_V90_RETRAINS;	/* +0x460 */
	int  	MAX_NOF_REMOTE_RETRAINS;	/* +0x464 */
	int  	RETRAIN_COUNTER_FADE_COUNT;	/* +0x468 */
	int  	REMOTE_RRN_COUNTER_FADE_COUNT;	/* +0x46c */
	int  	MINIMUM_DURATION_IN_DATA_BEFORE_RRN_UP;	/* +0x470 */
	int  	MINIMUM_DURATION_IN_DATA_BEFORE_RRN_DOWN;	/* +0x474 */
	int  	MINIMUM_DURATION_IN_DATA_BEFORE_EC_RRN;	/* +0x478 */
	int  	MAX_NOF_RATES_DIFF_BEFORE_RETRAIN;	/* +0x47c */
	int  	ENABLE_ERROR_CORRECTION_RRN;	/* +0x480 */
	float	EIA6_PDSNR_THRESHOLD_IN_PHASE4;	/* +0x484 */
	float	EIA6_PDSNR_THRESHOLD_IN_PHASE3;	/* +0x488 */
	float	EIA6_TRN1D_ERROR_FOR_V34_FALLBACK;	/* +0x48c */
	int  	EIA6_MAX_NOF_V90_RETRAINS;	/* +0x490 */
	int  	ENABLE_DROP_2_V34_ON_SEVERE_CODEC;	/* +0x494 */
	int  	DEBUG_DIGITAL_MODEM_INITIATE_RRN;	/* +0x498 */
	int  	DEBUG_DIGITAL_MODEM_INITIATE_RRN_TIME;	/* +0x49c */
	int  	TRN1_QC_DD_LENGTH;	/* +0x4a0 */
	int	unnamed_4a4;		/* +0x4a4  setToDefault only */
	int	unnamed_4a8;		/* +0x4a8  setToDefault only */
	int	unnamed_4ac;		/* +0x4ac  setToDefault only */
	int	unnamed_4b0;		/* +0x4b0  setToDefault only */
	int	unnamed_4b4;		/* +0x4b4  setToDefault only */
	int	unnamed_4b8;		/* +0x4b8  setToDefault only */
	int	unnamed_4bc;		/* +0x4bc  setToDefault only */
	int  	TRN2D_QC_DD_LENGTH;	/* +0x4c0 */
	int  	LINEAR_EQU_QC_TRN1D_FREEZE_DURATION;	/* +0x4c4 */
	int  	DFE_QC_TRN1D_FREEZE_DURATION;	/* +0x4c8 */
	int  	ANSPCM_DEMODULATION_LENGTH;	/* +0x4cc */
	int  	QC_LOGGING_PERIOD_INITIAL;	/* +0x4d0 */
	int  	QC_LOGGING_PERIOD_STEADY_STATE;	/* +0x4d4 */
	float	ANSPCM_CORRELATION_THRESH_FOR_VALIDATION;	/* +0x4d8 */
	int  	DEBUG_CONNECTION_EVALUATOR_ALTERNATE_DEBUG;	/* +0x4dc */
	int  	DEBUG_CONNECTION_EVALUATOR_FALL_BACK;	/* +0x4e0 */
	int  	DEBUG_CONNECTION_EVALUATOR_RETRAIN;	/* +0x4e4 */
	int  	DEBUG_CONNECTION_EVALUATOR_RATE_UP;	/* +0x4e8 */
	int  	DEBUG_CONNECTION_EVALUATOR_RATE_DOWN;	/* +0x4ec */
	int  	DEBUG_CONNECTION_EVALUATOR_PERIOD;	/* +0x4f0 */
	int  	HIGH_LEVEL_TX_ACTIVE;	/* +0x4f4 */
	int  	SENSITIVE_ISP_DETECTED;	/* +0x4f8 */
	int  	MAX_TX_RATE_INDEX_FOR_SENSITIVE_ISP;	/* +0x4fc */
	int  	LOOP_TYPE;	/* +0x500 */
	int  	SEPARATE_PHASE3_CONSTELLATIONS;	/* +0x504 */
	int  	SEPARATE_PHASE4_CONSTELLATIONS;	/* +0x508 */
	int  	SEPARATE_DATA_PHASE_CONSTELLATIONS;	/* +0x50c */
	int  	WRITE_TIMING_PHASE_AND_OFFSET_TO_FILE;	/* +0x510 */
	int  	WRITE_ERROR_TO_FILE;	/* +0x514 */
	int  	WRITE_DEMOD_IN_SAMPLES_TO_FILE;	/* +0x518 */
	int  	WRITE_EQU_COEFS_TO_FILE;	/* +0x51c */
	int  	LOAD_EQU_COEFS_FROM_FILE;	/* +0x520 */
	int  	DEBUG_PRINT_MAPPER_CONSTELLATIONS;	/* +0x524 */
	int  	DEBUG_PRINT_DEMAPPER_CONSTELLATIONS;	/* +0x528 */
	int  	DEBUG_DEMAPPER_ERROR_HISTOGRAM;	/* +0x52c */
	int  	DEMAPPER_DELAY_BEFORE_ERROR_HISTOGRAM;	/* +0x530 */
	int  	DEMAPPER_ERROR_HISTOGRAM_INTEGRATION_TIME;	/* +0x534 */
	int  	TEMP_INT_PARAMETER1;	/* +0x538 */
	int  	TEMP_INT_PARAMETER2;	/* +0x53c */
	int  	TEMP_INT_PARAMETER3;	/* +0x540 */
	int  	TEMP_INT_PARAMETER4;	/* +0x544 */
	float	TEMP_FLOAT_PARAMETER1;	/* +0x548 */
	float	TEMP_FLOAT_PARAMETER2;	/* +0x54c */
	float	TEMP_FLOAT_PARAMETER3;	/* +0x550 */
	float	TEMP_FLOAT_PARAMETER4;	/* +0x554 */
};

/*
 * THE RAW VIEW, which is what V90PreFilter.h used to provide by DEFINING A
 * SECOND V90Parameters -- a different class, of a different size, under the
 * same name (finding F1112).  That is undefined behaviour the moment both
 * reach one translation unit, and it was doing real damage: the two sizes
 * were 0x504 and 0x558, so a translation unit holding the smaller one and
 * allocating from `sizeof` under-allocated by 84 bytes, and
 * `tools/whichfield.py` -- the tool CLAUDE.md points you at to turn a
 * differential offset into a diagnosis -- resolved every offset of this class
 * to `b[8] (unsigned char)` and told you nothing.
 *
 * This is a VIEW rather than a rival: one class, one size, and the word and
 * float arrays laid over it for the regions that are not modelled as fields
 * yet.  The accessors below keep every existing `p->w[0x1c0 / 4]` spelling
 * working, including the ones indexed by a variable or a symbolic constant.
 *
 * A SITE THAT USES THIS IS A SITE WITH WORK LEFT IN IT.  Where the offset
 * lands on a field this header already names, the named field is the better
 * spelling -- and for the forty-odd field-to-field copies in
 * `V90PreFilter::setParamEia6` it is arguably the CORRECT one: those copy
 * `float` parameters, and the int view forces the integer `mov` that finding
 * F1242 says is a spelling to avoid reaching for, because it fits the compiler
 * rather than recording the source.  `make period` can now adjudicate that,
 * which it could not when 1242 was written.  Task #116.
 */
union V90ParamsRaw {
	unsigned char	b[0x558];
	int		w[0x558 / 4];
	float		f[0x558 / 4];
};

/*
 * Guarded on a 32-bit pointer for the reason every offset assertion in this
 * tree is: the layout the blob has is a 32-bit layout, and `make check64`
 * lays the class out differently because it holds pointers.  Asserting the
 * equality on a host that cannot have it is asserting the wrong thing.
 */
#if defined(__SIZEOF_POINTER__) && __SIZEOF_POINTER__ == 4
typedef char v90pr_size[(sizeof(union V90ParamsRaw)
			 == sizeof(V90Parameters)) ? 1 : -1];
#endif

#define V90PB(p)	(((union V90ParamsRaw *)(p))->b)
#define V90PW(p)	(((union V90ParamsRaw *)(p))->w)
#define V90PF(p)	(((union V90ParamsRaw *)(p))->f)

#endif /* DSPLIB_V90PARAMETERS_H */

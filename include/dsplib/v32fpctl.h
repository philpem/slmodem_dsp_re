/*
 * v32fpctl.h -- ITU-T V.32 / V.32bis: the datapump's control surface.
 *
 * Twenty-one small functions that the handshake's state machines and the
 * data-mode loop call to reconfigure the datapump: the two mode setters, the
 * three adaptation switches, the echo canceller's round-trip delay, the two
 * event pollers, and a handful of accessors.  Nothing here does signal
 * processing; every one of them moves constants into sub-objects other files
 * already model.
 *
 * `make worklist` brackets most of them into `V32mod.c +39` and the rest into
 * `Dialer.c +18`.  Both are SPANS OF THE BLOB'S LAYOUT, not module names --
 * v32data.h makes the same point -- and every function here reaches the V.32
 * datapump through `V32_OBJ_FP`, so they are V.32 and not dialling.
 *
 * ---------------------------------------------------------------------------
 * THE INSTANCE IS NOT MODELLED AS A STRUCT.
 *
 * The parameter is `void *` and the offsets are named constants, which is
 * v32data.h's ruling and v22data.h's before it.  What IS modelled, by other
 * headers, is every sub-object these functions reach:
 *
 *   fp + 0x30    struct v32_sdm      the transmit scrambler   (v32scram.h)
 *   fp + 0x48    struct v32_smc      the symbol coder         (v32smc.h)
 *   fp + 0x60    struct fpm_pps      the pulse shaper         (fpm_pps.h)
 *   fp + 0xb0    struct v32_symout   the symbol ring          (v32smc.h)
 *   fp + 0xe0    struct fpm_ecc      the echo canceller       (fpm_ecc.h)
 *   fp + 0x204   struct fpm_fse      the equaliser/slicer     (fpm_fse.h)
 *   fp + 0x50b0  struct v32_sdm      the receive descrambler  (v32scram.h)
 *   fse->cfg.owner  struct v32_dec   the decoder's context    (v32dec.h)
 *
 * Two of those eight were not asserted anywhere before this file and are what
 * makes the mode setters readable rather than a wall of offsets:
 *
 *   - `ScrambleDataV32` hands fp + 0x30 to `SDMv32_scrambler` and
 *     `DescrambleDataV32` hands fp + 0x50b0 to `SDMv32_descrambler`, so both
 *     are `struct v32_sdm`.  Every field `SetTxModeV32` writes below fp + 0x48
 *     then lands on a named member of it, and the same seven fields appear in
 *     `SetRxModeV32` at exactly +0x5080 more.
 *   - `V32FP_GetDiagnostics` hands fp + 0x204 to `FSE_getdiag` and
 *     `V32FP_delete` hands the same address to `FPM_FSE_free`, so the
 *     equaliser is there; fp + 0x230 is then `cfg.owner`, fp + 0x234 is
 *     `cfg.decision` -- which is why the mode setter stores an
 *     `FSE_decision_*` into it -- and fp + 0x23c is `mu_sel`.
 *
 * Findings F8210 and F8211.
 */

#ifndef DSPLIB_V32FPCTL_H
#define DSPLIB_V32FPCTL_H

#include "dsplib/v32data.h"		/* V32_OBJ_FP and the fp offsets it owns */

#ifdef __cplusplus
extern "C" {
#endif

struct fpm_tone;
struct fpm_fse_point;

/* ------------------------------------------------------------------------ */
/* The instance.                                                            */

/*
 * A reason code, and the flag beside it.
 *
 * `SetTxModeV32` and `SetRxModeV32` write 0x17 here when the requested mode is
 * out of range; `RxHdxNull` writes 0x10 to the same byte on a different fault
 * and sets the same flag, which is what says the byte is a code rather than a
 * state.  The flag's meaning is USAGE INFERENCE and nothing stronger: every
 * site that sets it also writes the byte, so the reading is "a reason has been
 * posted".
 */
#define V32_OBJ_STATUS		0x30	/* unsigned char                      */
#define V32_OBJ_FLAGS		0x31	/* unsigned char                      */
#define V32_STATUS_BAD_MODE	0x17
#define V32_FLAG_FAULT		0x02

#define V32_OBJ_BPS		0x04	/* short, in bit/s; GetRateV32        */
/*
 * `SetAdaptEcV32` mode 0 copies this into `fpm_ecc::near_delay`, which
 * fpm_ecc.h records as an INPUT that init reads and never writes.
 */
#define V32_OBJ_EC_NEAR_DELAY	0x14	/* unsigned short                     */
#define V32_OBJ_SYMLEN_SEL	0x18	/* short; indexes V32_SYMBOL_LEN      */
/*
 * Non-zero selects the trellis variant at 9600: `GetRateV32` reports
 * V32_RATE_9600 when it is set and V32_RATE_9600_NT when it is clear, and
 * those are exactly the two 9600 arms of the mode setters.
 */
#define V32_OBJ_TRELLIS		0x1c	/* int                                */

/*
 * Guarded: `include/dsplib/v32seq.h` defines the same offset from the sequence
 * generator's side, and the two headers have to be includable together.
 */
#ifndef V32_OBJ_HDX
#define V32_OBJ_HDX		0x64	/* the half-duplex / handshake context */
#endif
/* V32_OBJ_FP is 0x68 and belongs to v32data.h. */

/* ------------------------------------------------------------------------ */
/* The half-duplex context, at V32_OBJ_HDX.                                 */

/*
 * Three tone objects and a multi-tone detector.  `V32FP_delete` frees all four
 * -- +0x38 with `FPM_MTD_delete` and the other three with `FPM_TONE_delete` --
 * and `SetToneDetect` retunes the first.
 */
#define V32_HDX_TONE0		0x2c	/* struct fpm_tone *                  */
#define V32_HDX_TONE1		0x30	/* struct fpm_tone *                  */
#define V32_HDX_TONE2		0x34	/* struct fpm_tone *                  */
#define V32_HDX_MTD		0x38	/* struct fpm_mtd *                   */

/*
 * MODELLED, UNNAMED -- four 16-bit timing quantities.
 *
 * `CalcTurnAroundDelay` returns +0x94 minus the sum of the other three,
 * clamped at zero, so +0x94 is a budget and +0x98/+0x9a/+0x9c are charges
 * against it; `SetECRndTripDelayV32` adds +0x9c into the echo canceller's
 * delay-line length, so that one at least is a delay in symbols.  What each
 * individually IS is not established here and is not guessed at.
 */
#define V32_HDX_SHORT_94	0x94
#define V32_HDX_SHORT_98	0x98
#define V32_HDX_SHORT_9A	0x9a
#define V32_HDX_SHORT_9C	0x9c
/*
 * MODELLED, UNNAMED -- `RxClampV32` writes this many words and returns it, and
 * `RxHdxNull` accumulates it into +0x7c against a limit at +0x80.  Both are
 * consistent with "symbols in one receive block"; that is usage inference, so
 * the name stays neutral.
 */
#define V32_HDX_SHORT_9E	0x9e
#define V32_HDX_BUF_A4		0xa4	/* a heap buffer; freed by delete     */

/* ------------------------------------------------------------------------ */
/* The datapump block, at V32_OBJ_FP.  v32data.h owns 0x48, 0x60, 0x98..0xc0. */

/*
 * Seven `int` adaptation switches in one bank, the same shape V.22 has at
 * `v22fp_dsp` +0x00..+0x18.
 *
 * +0x10 is NAMED and the other six are not: `SetAdaptEqV32` writes +0x10
 * exactly as `SetAdaptEqV22` writes `v22fp_dsp::eq_adapt`, at the same offset
 * in the same bank, from a function of the same name and the same three-mode
 * shape.  `SetRxLoopsV32` drives +0x00..+0x0c as one group and `SetAdaptEcV32`
 * drives +0x14 and +0x18 as another, which bounds them but does not name them.
 */
#define V32FP_R00		0x00
#define V32FP_R04		0x04
#define V32FP_R08		0x08
#define V32FP_R0C		0x0c
#define V32FP_EQ_ADAPT		0x10
#define V32FP_R14		0x14
#define V32FP_R18		0x18

/*
 * MODELLED, UNNAMED -- one short per direction, set by the mode setters to
 * 0, 0, 1, 1, 3, 4, 5 across the seven modes.
 *
 * THE NEAREST READING FITS SIX OF THE SEVEN AND IS THEREFORE DECLINED.  Those
 * values are `GetRateV32`'s own rate codes on every mode but one: 14400 -> 5,
 * 12000 -> 4, 7200 -> 3, and an unrecognised rate -> 0, which is what the two
 * 4800 arms leave.  **The exception is 9600.**  `GetRateV32` splits it --
 * V32_RATE_9600_NT (1) without the trellis and V32_RATE_9600 (2) with it -- and
 * both 9600 arms of `SetTxModeV32` write 1 here, the trellis one by falling
 * into the other's store.  So the value 2 never appears at either offset in
 * this batch, and a field that collapses the two 9600 arms is not that
 * encoding however well it correlates on the other six.
 *
 * `V32FP_recreate` and `V32FP_control` both touch this neighbourhood and
 * neither is reconstructed, so whether anything ever writes 2 here is open.
 * Until it is settled the name stays neutral: a six-of-seven fit is exactly
 * the case CLAUDE.md rates worse than leaving it padded.
 *
 * (`fp + 0x28` and `fp + 0x2a` ARE the transmit and receive rate indices,
 * derived separately from `V32FP_recreate`'s own configuration dump.  They are
 * a different pair, two bytes below these, and `obj + 0x2a` is a third thing
 * again -- `energy_drop_time`.  Three quantities within four bytes of one
 * another across two blocks, so read the base register before the offset.)
 */
#define V32FP_SHORT_2C		0x2c	/* transmit                           */
#define V32FP_SHORT_2E		0x2e	/* receive                            */

#define V32FP_SCRAMBLER		0x30	/* struct v32_sdm                     */
#define V32FP_MRF		0xc4	/* struct fpm_mrf                     */
#define V32FP_ECC		0xe0	/* struct fpm_ecc                     */
#define V32FP_SRE		0x148	/* struct fpm_sre                     */
#define V32FP_FSE		0x204	/* struct fpm_fse                     */

/* MODELLED, UNNAMED -- two receive-side shorts the mode setter drives. */
#define V32FP_SHORT_5098	0x5098
#define V32FP_SHORT_509C	0x509c

#define V32FP_DESCRAMBLER	0x50b0	/* struct v32_sdm                     */

#define V32FP_BUF_5038		0x5038	/* heap buffers, freed by delete      */
#define V32FP_BUF_50CC		0x50cc
/*
 * `V32FP_GetCleanedSamples` hands the caller the buffer at +0x50d0 and the
 * count at +0x50d4, and reports zero rather than the count when the count is
 * above V32FP_CLEAN_MAX.  The names are the accessor's own word.
 */
#define V32FP_CLEAN_BUF		0x50d0	/* short *                            */
#define V32FP_CLEAN_N		0x50d4	/* unsigned short                     */
#define V32FP_CLEAN_MAX		0xa0

/* ------------------------------------------------------------------------ */
/* Inside `struct v32_sdm`, at V32FP_SCRAMBLER and V32FP_DESCRAMBLER.        */

/*
 * The two ABSOLUTE feedback tap positions.
 *
 * v32scram.h names +0x14 and +0x16 `tap1` and `tap2` and records that both are
 * RIGHT SHIFTS applied to the register.  The mode setters compute them as
 * `position - shift`, reading the two positions from +0x02 and +0x04 as
 * unsigned 16-bit quantities -- so those two are the taps as the Recommendation
 * states them, and the stored `tap1`/`tap2` are what is left after the group
 * shift is taken out.
 *
 * v32scram.h has +0x02 as `pad02` ("not read here") and +0x04..+0x07 as one
 * `int pad04`; the second is read here as a 16-bit field, which that header
 * could not have known.  Finding F8212.  These are byte offsets rather than
 * members for exactly that reason: the type is not this file's to change.
 */
#define V32_SDM_TAP1_POS	0x02	/* unsigned short                     */
#define V32_SDM_TAP2_POS	0x04	/* unsigned short                     */

/* ------------------------------------------------------------------------ */
/* Inside `struct v32_dec`, which is `fpm_fse_cfg::owner`.                   */

/*
 * `v32dec.h` has +0x62 as `retrain`, "1 or 2; a request, not a state -- nothing
 * here reads it back".  These two are what reads it back, and they name the
 * bits: `RetrainDetectV32` takes bit 0 and `RenegotiateDetectV32` bit 1, each
 * clearing its own bit and reporting 1.
 */
#define V32_DEC_RETRAIN_REQ	0x01
#define V32_DEC_RENEG_REQ	0x02

/*
 * MODELLED, UNNAMED -- +0x04 falls inside v32dec.h's `pad02[6]` and +0x74 is
 * one past the end of that struct.  `SetRxModeV32` writes the first and
 * `RetrainDetectV32` increments the second; both are recorded as offsets here
 * rather than added to a header this batch does not own.  Finding F8213.
 */
#define V32_DEC_SHORT_04	0x04
#define V32_DEC_RETRAIN_N	0x74	/* unsigned short                     */

/* ------------------------------------------------------------------------ */
/* The adaptation setters' mode words.                                      */

/*
 * `SetAdaptEqV32`.  MU0 and MU1 both switch adaptation on and differ only in
 * which entry of `fpm_fse_cfg::mu[]` the LMS update takes its step from, which
 * is what `fpm_fse::mu_sel` selects -- so the names are the field's, not a
 * reading of what "wide" and "narrow" mean.  Any other value does nothing.
 */
#define V32_ADAPTEQ_OFF		1
#define V32_ADAPTEQ_MU0		2
#define V32_ADAPTEQ_MU1		3

/*
 * `SetAdaptEcV32`.  RESET re-runs `FPM_ECC_init` in place; ON clears the delay
 * line and enables both update loops; SLOW divides the update gain by ten and
 * touches nothing else.  Any other value does nothing.
 */
#define V32_ADAPTEC_RESET	0
#define V32_ADAPTEC_OFF		1
#define V32_ADAPTEC_ON		2
#define V32_ADAPTEC_SLOW	3

/* ------------------------------------------------------------------------ */
/* Line rates, as `GetRateV32` reports them.                                 */

/*
 * The codes come from `GetRateV32` mapping `V32_OBJ_BPS`; the modulation
 * against each is what the mode setter installs for the corresponding
 * V32_MODE_*, which is one higher throughout.
 */
#define V32_RATE_4800		0	/* 4-point,   differential           */
#define V32_RATE_9600_NT	1	/* 16-point,  differential, no TCM   */
#define V32_RATE_9600		2	/* 32-point,  trellis                */
#define V32_RATE_7200		3	/* 16-point,  trellis                */
#define V32_RATE_12000		4	/* 64-point,  trellis                */
#define V32_RATE_14400		5	/* 128-point, trellis                */
#define V32_RATE_INVALID	6	/* `V32_OBJ_BPS` matched none of them */

/* ------------------------------------------------------------------------ */
/* Modulation modes, as the two setters take them.                          */

/*
 * NAMED FROM WHAT EACH ARM INSTALLS, which is three independent statements per
 * arm and not one: the constellation map pair (`SMCv32_?MAP16` or one of the
 * four `VTBv32_?MAP*`), the scrambler group width, and -- for the trellis arms
 * -- the uncoded-bit count in `v32_smc`'s +0x14.  Those agree with each other
 * and with V.32bis's rate table on every mode.
 *
 * V32_MODE_ABS4 and V32_MODE_DIF4 share an arm and differ in exactly two
 * stores, both of which select the absolute rather than the differential
 * encoder, so the "ABS"/"DIF" halves of those names are the encoder selector's
 * own doing (v32data.h: 0 dif, 1 abs, 2 tcm).
 */
#define V32_MODE_ABS4		0	/* 4-point,   absolute mapping       */
#define V32_MODE_DIF4		1	/* 4-point,   differential;   4800   */
#define V32_MODE_16		2	/* 16-point,  no trellis;     9600   */
#define V32_MODE_32T		3	/* 32-point,  trellis;        9600   */
#define V32_MODE_16T		4	/* 16-point,  trellis;        7200   */
#define V32_MODE_64T		5	/* 64-point,  trellis;       12000   */
#define V32_MODE_128T		6	/* 128-point, trellis;       14400   */
#define V32_MODE_COUNT		7

/* ------------------------------------------------------------------------ */

/*
 * The symbol ring's length, indexed by `V32_OBJ_SYMLEN_SEL`.
 *
 * `.data` and not `.rodata` in the object, so it is not declared const even
 * though nothing writes it.  Two entries: `V32FP_recreate` reads it for
 * `v32_symout::limit`, `V32FP_control` reads it, and `SetECRndTripDelayV32`
 * reads it as the symbol period the echo canceller's taps are placed in.
 */
extern short V32_SYMBOL_LEN[2];

/*
 * Its two neighbours, twelve contiguous `.data` bytes with it and indexed by
 * the same selector at every site.  `V32_SAMPLE_LEN` reaches hdx + 0xa0
 * (v32hdx.h's V32HDX_SAMPLE_LEN) from both `V32FP_recreate` and
 * `V32FP_control`, and `V32_TURNAROUND_DLY` reaches hdx + 0x94, which
 * v32fpctl.h describes below as the budget `CalcTurnAroundDelay` spends.
 * `src/pump/v32/v32fptab.c` has the layout note.
 */
extern short V32_TURNAROUND_DLY[2];
extern short V32_SAMPLE_LEN[2];

/* ------------------------------------------------------------------------ */

/**
 * @brief Tear a V.32 datapump instance down.
 *
 * Frees every sub-object, every heap buffer, the half-duplex context, the
 * datapump block, and finally the instance itself.
 *
 * It re-reads `V32_OBJ_HDX` and `V32_OBJ_FP` before every single use --
 * thirteen reloads of two fields, which is what the object does and what
 * a caller that cached them would not.
 *
 * @param modem  The V.32 datapump instance to tear down.
 */
void V32FP_delete(void *modem);

/**
 * @brief Drain the V.32 equaliser's scatter log.
 *
 * A wrapper over `FSE_getdiag(fp + 0x204, ...)`.
 *
 * @param modem  The V.32 datapump instance.
 * @param which  Forwarded to `FSE_getdiag` unchanged.
 * @param out    Output for at most @p max scatter points.
 * @param max    Maximum points to write.
 * @return The number of points written.
 */
int V32FP_GetDiagnostics(void *modem, int which, struct fpm_fse_point *out,
			 int max);

/**
 * @brief Hand back the V.32 echo-cancelled input block.
 *
 * A count above V32FP_CLEAN_MAX is reported as none, and the buffer is
 * returned either way.
 *
 * @param modem  The V.32 datapump instance.
 * @param n      Output: how many samples are in the returned block.
 * @return The echo-cancelled sample buffer (V32FP_CLEAN_BUF).
 */
short *V32FP_GetCleanedSamples(void *modem, int *n);

/**
 * @brief Configure the V.32 transmitter for one of the seven modulations.
 *
 * Out of range -- and the test is unsigned against 6 after a sign
 * extension, so a negative mode is out of range too -- posts
 * V32_STATUS_BAD_MODE and leaves everything but the scrambler's derived
 * fields alone.
 *
 * @param modem  The V.32 datapump instance.
 * @param mode   One of V32_MODE_ABS4..V32_MODE_128T.
 */
void SetTxModeV32(void *modem, short mode);

/**
 * @brief Configure the V.32 receiver for one of the seven modulations, mode for mode with SetTxModeV32().
 * @param modem  The V.32 datapump instance.
 * @param mode   One of V32_MODE_ABS4..V32_MODE_128T.
 */
void SetRxModeV32(void *modem, short mode);

/**
 * @brief Load the V.32 transmit scrambler's shift register.
 * @param modem  The V.32 datapump instance.
 * @param seed   The new register contents.
 */
void SeedScramblerV32(void *modem, unsigned int seed);

/**
 * @brief Map the V.32 instance's current bit rate to a V32_RATE_* code.
 * @param modem  The V.32 datapump instance.
 * @return One of V32_RATE_4800..V32_RATE_INVALID (V32_OBJ_TRELLIS breaks the 9600 tie).
 */
int GetRateV32(void *modem);

/**
 * @brief Scramble `count` V.32 transmit words in place.
 * @param modem  The V.32 datapump instance.
 * @param buf    The words to scramble, in place.
 * @param count  How many words.
 */
void ScrambleDataV32(void *modem, short *buf, unsigned short count);

/**
 * @brief Descramble `count` V.32 receive words in place.
 * @param modem  The V.32 datapump instance.
 * @param buf    The words to descramble, in place.
 * @param count  How many words.
 */
void DescrambleDataV32(void *modem, short *buf, unsigned short count);

/**
 * @brief Control V.32 equaliser adaptation.
 * @param modem  The V.32 datapump instance.
 * @param mode   V32_ADAPTEQ_OFF, _MU0 (wide step) or _MU1 (narrow step); anything else is a no-op.
 */
void SetAdaptEqV32(void *modem, unsigned short mode);

/**
 * @brief Control the V.32 echo canceller's adaptation.
 * @param modem  The V.32 datapump instance.
 * @param mode   V32_ADAPTEC_RESET/_OFF/_ON/_SLOW; anything else is a no-op.
 */
void SetAdaptEcV32(void *modem, unsigned short mode);

/**
 * @brief Set the four receive-loop switches at V32FP_R00..V32FP_R0C.
 * @param modem  The V.32 datapump instance.
 * @param mode   1 clears them; 2 and 3 set them.
 */
void SetRxLoopsV32(void *modem, unsigned short mode);

/**
 * @brief Place the V.32 echo canceller's read taps for a given round-trip delay.
 *
 * Resizes its delay line, points the symbol ring at the new geometry, and
 * presets the line to the no-carrier constellation index.
 *
 * @param modem  The V.32 datapump instance.
 * @param delay  The round-trip delay, in symbols.
 */
void SetECRndTripDelayV32(void *modem, short delay);

/**
 * @brief A `ret`, and nothing else. Its one caller passes the instance.
 * @param modem  Unused.
 */
void TxClockSyncV32(void *modem);

/**
 * @brief Read the V.32 decoder's rate-change report, straight through.
 * @param modem  The V.32 datapump instance.
 * @return The decoder's `struct v32_dec::rate_change`.
 */
int EpochDetectV32(void *modem);

/**
 * @brief Poll and clear the V.32 decoder's retrain request.
 *
 * Unlike RenegotiateDetectV32(), a hit also counts (V32_DEC_RETRAIN_N).
 *
 * @param modem  The V.32 datapump instance.
 * @return Non-zero if a retrain was requested (and now cleared).
 */
int RetrainDetectV32(void *modem);

/**
 * @brief Poll and clear the V.32 decoder's renegotiate request.
 * @param modem  The V.32 datapump instance.
 * @return Non-zero if a renegotiation was requested (and now cleared).
 */
int RenegotiateDetectV32(void *modem);

/**
 * @brief Fill `out` with V32_HDX_SHORT_9E clamped words, in the shape of a V32 receive-state handler.
 *
 * @p in and @p count are never read -- the signature is the Rx state
 * functions' shape, taken from `RxHdxNull`'s call and no stronger than
 * that.
 *
 * @param modem  The V.32 datapump instance.
 * @param in     Unread.
 * @param out    Output for V32_HDX_SHORT_9E clamped words.
 * @param count  Unread.
 * @return The number of words written (V32_HDX_SHORT_9E).
 */
unsigned short RxClampV32(void *modem, short *in, short *out,
			  unsigned short count);

/**
 * @brief Placeholder for a file-local, byte-sized `ret` stub in `V32_PROTOCOL`'s table.
 *
 * FILE-LOCAL IN THE OBJECT, so it is `static` in `src/pump/v32/v32fpdisp.c`,
 * the unit that holds `V32_PROTOCOL`, and has no declaration here.  Nothing in
 * `.text` calls it; the reference is from the table.  It is one byte of `ret`,
 * so its signature is not recoverable and `void (void)` is a placeholder
 * rather than a reading.  A test names it through the test tier's globalized
 * copies (tools/testvisible.py).
 */

/**
 * @brief Retune the V.32 half-duplex context's first tone detector.
 *
 * Hands `FPM_TONE_create` a copy of the object's own current
 * configuration with the frequency replaced.
 *
 * @param modem  The V.32 datapump instance.
 * @param hz     The new frequency.
 */
void SetToneDetect(void *modem, short hz);

/**
 * @brief What is left of the V.32 handshake's turnaround budget once the three charges against it are taken out.
 * @param modem  The V.32 datapump instance.
 * @return The remaining budget (V32_HDX_SHORT_94 minus the three charges), clamped at zero.
 */
short CalcTurnAroundDelay(void *modem);

#ifdef __cplusplus
}
#endif

#endif /* DSPLIB_V32FPCTL_H */

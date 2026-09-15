/*
 * v22prc.h -- V.22 / V.22bis: the small state and status helpers.
 *
 * Nine leaf functions from two of the author's translation units, grouped
 * here because they share one property: none of them calls anything, so all
 * nine could be written and tested before the datapump object they read is
 * modelled.  (Three more joined in the 2026-08-30 leaf batch --
 * `V22FP_control`, `ScramblerOn`, `DescramblerOn`, no-entry-point exports
 * from the same neighbourhood -- so the counts in this header's history
 * below are of the original nine-then-ten, not of the file today.)
 *
 * These offsets were derived one function at a time before the V.22 datapump
 * instance was modelled.  They now resolve to fields of `struct v22fp` and
 * its nested blocks; the implementation uses those typed owners directly.
 * The public `void *` parameters and these constants remain API and
 * differential-fixture anchors.
 *
 * The offsets are not guesses; each is a load or a store in the object, and
 * the function's own name is what licenses the reading of it.
 *
 * ---------------------------------------------------------------------------
 * `V22FP_create` HAS NOW LANDED, and all ten of these resolve.
 *
 * `include/dsplib/v22fp.h` is the struct, and the bottom of
 * `src/pump/v22/v22fp.c` holds every constant below as a compile-time
 * assertion against it -- so the two spellings cannot drift apart, and the
 * fact that ten offsets derived from ten unrelated instructions each land on
 * a field is corroboration neither file could have produced alone:
 *
 *   V22_OBJ_GTIMER  -> struct v22fp::hdx, and `hdx->gtimer` beyond it
 *   V22_OBJ_FP      -> struct v22fp::dsp
 *   V22FP_EQ_ADAPT  -> dsp->eq_adapt          create leaves 1
 *   V22FP_TX_CLOCK  -> dsp->pps.cfg.step
 *   V22FP_SIGNAL    -> dsp->agc.signal
 *   V22FP_BAUD      -> dsp->sre.pll_acc
 *   V22FP_CARRIER   -> dsp->sre.active
 *   V22FP_EQ_MODE   -> dsp->fse.mu_sel        init 0
 *   V22FP_EQ_EXTRA  -> dsp->fse.lms_on        init 1
 *   V22FP_QUALITY   -> dsp->fse.mse           init 0
 *
 * THE CONSTANTS STAY ANYWAY, and the reason is not inertia.  Four of the ten
 * resolve INSIDE another module's struct -- three into `struct v22_fse` and
 * one into `struct v22_sre` -- so retyping these nine functions would mean
 * writing `fp->dsp->fse.r22` where the name `GetSignalQuality` is the only
 * evidence, i.e. carrying this file's names into headers whose own rule is
 * that a field is named only where an instruction in ITS module forces it.
 * Two of the ten are also an open question rather than a resolution:
 * `V22FP_TX_CLOCK` lands on a configuration word `V22_PPS_init` copies in and
 * `V22_PPS_filter` reads on every output, so `TxClockSync` is overwriting a
 * configuration field after initialisation -- deliberate or a soft spot in one
 * of the two readings, and nothing here decides it.  `V22FP_BAUD` has the same
 * shape.  Retiring the constants would bury both questions inside a name.
 */

#ifndef DSPLIB_V22PRC_H
#define DSPLIB_V22PRC_H

/*
 * Offsets in the datapump instance -- the pointer these functions are handed.
 */
#define V22_OBJ_GTIMER		0x50	/* int *, a shared millisecond clock  */
#define V22_OBJ_FP		0x54	/* the V22FP receiver/transmitter     */

/*
 * Offsets in the object at V22_OBJ_FP.  Named after the function that reads
 * each, which is the only evidence there is for what they mean.
 */
#define V22FP_TX_CLOCK		0x78	/* short, written by TxClockSync      */
#define V22FP_SIGNAL		0xec	/* int,   returned by SignalDetect    */
#define V22FP_BAUD		0x12a	/* short, tripled into TX_CLOCK       */
#define V22FP_CARRIER		0x130	/* int,   returned by CarrierDetect   */
#define V22FP_QUALITY		0x186	/* unsigned short, GetSignalQuality   */

/* Written only by SetAdaptEqV22; see the note on its three modes. */
#define V22FP_EQ_ADAPT		0x10	/* int                                */
#define V22FP_EQ_MODE		0x16c	/* short                              */
#define V22FP_EQ_EXTRA		0x180	/* int, mode 3 only                   */

/*
 * The pair `ScramblerOn` / `DescramblerOn` return, and `V22FP_control`
 * writes from bits 0 and 1 of its control byte.  In struct v22fp_dsp these
 * are r18 and r1c, seeded by create from params.flags bits 0 and 1; the two
 * accessor names are the evidence for what the pair indicates.
 */
#define V22FP_SCRAMBLER_ON	0x18	/* int */
#define V22FP_DESCRAMBLER_ON	0x1c	/* int */

/*
 * The rest of what V22FP_control touches.  Meanings are NOT established --
 * each name records where the value comes from, not what it does:
 *
 *   V22FP_CTL_BIT2   dsp r20, <- control byte 0xc bit 2 (create seeds it
 *                    from params.flags bit 2)
 *   V22FP_AGC_F18    dsp->agc.f18 -- fpm_agc.h: "set to 1 by init on reset;
 *                    agc() never reads it, a caller must".  V22FP_control is
 *                    that caller's other half: it writes !(byte 0xc bit 3).
 *   V22FP_FLAGS_B1   byte 1 of params.flags (the object's +0x11); control
 *                    rewrites bit 1 of it, i.e. flags bit 9, from control
 *                    byte 0xc bit 7.
 *   V22HDX_R0C/R0E   the tone/detector context words v22fp.h names r0c and
 *                    r0e; control writes (1,6) or (0,4) from byte 0xd.
 */
#define V22FP_CTL_BIT2		0x20	/* int   */
#define V22FP_AGC_F18		0xe8	/* int   */
#define V22FP_FLAGS_B1		0x11	/* byte, in the OBJECT not the dsp   */
#define V22HDX_R0C		0x0c	/* short */
#define V22HDX_R0E		0x0e	/* short */

/*
 * The control block V22FP_control is handed.  Only bytes 0xc and 0xd are
 * read; nothing else about the block's shape is visible from here.
 */
#define V22CTL_FLAGS		0x0c	/* byte: bit 0 scrambler on, bit 1
					 * descrambler on, bits 2/3/7 above  */
#define V22CTL_MODE		0x0d	/* byte: bit 2 and bits 7:6 == 10
					 * drive the (r0c, r0e) pair         */

/*
 * The datapump's block, in samples.  TxNOP emits exactly this many and then
 * reports the count, so the constant is in the code and not a parameter --
 * the same 160 that v22_iir.c's filter loop runs.
 */
#define V22_TX_BLOCK		160

/* RxClampV22's block, which is a different and much shorter one. */
#define V22_CLAMP_BLOCK		12
#define V22_CLAMP_VALUE		15

/*
 * The two "have we finished training" predicates, and the patterns they look
 * for.  1200 requires EVERY entry to be 3; 2400 requires a run of more than
 * seven 15s at the END of the array.  The asymmetry is the original's.
 */
#define V22_TRAINED_1200_SYMBOL	3
#define V22_TRAINED_2400_SYMBOL	15
#define V22_TRAINED_2400_RUN	7	/* strictly more than this */

/**
 * @brief Advance the V.22 datapump's shared millisecond clock by one block.
 * @param modem  The V.22 datapump instance.
 * @return The clock's new value; the step is 20 ms (160 samples at 8 kHz).
 */
int ReadGTimer(void *modem);

/**
 * @brief Emit one block of silence, in the shape of a V22_PROTOCOL transmit-state handler.
 *
 * Writes V22_TX_BLOCK zero samples and the count. The first two arguments
 * are never read; their types are undetermined (`void *` is chosen for the
 * stack slot, not from evidence), and the shape is that of a transmit-state
 * handler, which is what the seven-entry V22_PROTOCOL table holds.
 *
 * @param modem  Unread.
 * @param arg1   Unread.
 * @param out    Output for V22_TX_BLOCK zero samples.
 * @param count  Output: set to V22_TX_BLOCK.
 */
void TxNOP(void *modem, void *arg1, short *out, short *count);

/**
 * @brief Emit V22_CLAMP_BLOCK samples of V22_CLAMP_VALUE, in the same shape as TxNOP().
 * @param modem  Unread.
 * @param arg1   Unread.
 * @param out    Output for V22_CLAMP_BLOCK copies of V22_CLAMP_VALUE.
 * @param count  Output: set to V22_CLAMP_BLOCK.
 */
void RxClampV22(void *modem, void *arg1, short *out, short *count);

/**
 * @brief Has the V.22 1200 bit/s trainer converged? Every symbol must equal V22_TRAINED_1200_SYMBOL.
 * @param symbols  The received symbols.
 * @param count    How many symbols, read through a pointer.
 * @return Non-zero if trained.
 */
int RxTrained1200(const short *symbols, const unsigned short *count);

/**
 * @brief Has the V.22bis 2400 bit/s trainer converged? Requires a run of more than V22_TRAINED_2400_RUN trailing V22_TRAINED_2400_SYMBOL entries.
 * @param symbols  The received symbols.
 * @param count    How many symbols, read through a pointer.
 * @return Non-zero if trained.
 */
int RxTrained2400(const short *symbols, const unsigned short *count);

/**
 * @brief Is a V.22 carrier currently detected?
 * @param modem  The V.22 datapump instance.
 * @return Non-zero if carrier is present (V22FP_CARRIER).
 */
int CarrierDetect(void *modem);

/**
 * @brief Is a V.22 signal currently detected?
 * @param modem  The V.22 datapump instance.
 * @return Non-zero if signal is present (V22FP_SIGNAL).
 */
int SignalDetect(void *modem);

/**
 * @brief Read the V.22 equaliser's current signal-quality number.
 * @param modem  The V.22 datapump instance.
 * @return The quality value at V22FP_QUALITY (see v22status.h for its scale).
 */
unsigned short GetSignalQuality(void *modem);

/**
 * @brief Derive the V.22 transmit pulse-shaper clock from the baud field.
 *
 * Stores three times the baud field (V22FP_BAUD) back as a short at
 * V22FP_TX_CLOCK.
 *
 * Declared void, and the object does not settle that: it leaves the
 * product in `eax`, which is what a `short`-returning function would also
 * do, and there is no extension either way to tell them apart. The store
 * is the same under both readings, so nothing observable turns on it;
 * recorded here so that a caller found later to use the value is
 * recognised as evidence rather than as a contradiction.
 *
 * @param modem  The V.22 datapump instance.
 */
void TxClockSync(void *modem);

/**
 * @brief Control V.22 equaliser adaptation.
 *
 * Three live modes and a silent default:
 *
 *   1  stop adapting              EQ_ADAPT = 0
 *   2  adapt                      EQ_ADAPT = 1, EQ_MODE = 0
 *   3  adapt, second mode         EQ_ADAPT = 1, EQ_MODE = 1, EQ_EXTRA = 1
 *   anything else                 nothing at all, silently
 *
 * @p mode is loaded with `movzwl`, so it is sixteen bits wide and
 * unsigned; a caller passing 0x10002 selects nothing, not mode 2. Note
 * also that mode 3 sets EQ_EXTRA and mode 2 does not clear it, so the two
 * are not symmetrical and the order the caller uses them in matters.
 *
 * @param modem  The V.22 datapump instance.
 * @param mode   One of the three modes above; anything else is a no-op.
 */
void SetAdaptEqV22(void *modem, unsigned short mode);

/*
 * V22FP_control, ScramblerOn and DescramblerOn are declared by
 * `dsplib/v22ctl.h`, against the modelled `struct v22fp` rather than the
 * `void *modem` this header uses.  They were reconstructed twice, in two
 * waves; `v22ctl.c` is their one home.  Include that header to call them.
 */

#endif /* DSPLIB_V22PRC_H */

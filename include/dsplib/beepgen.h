/*
 * beepgen.h -- the beep generator's DTMF frequency table and the float/linear
 * utility set that shares its translation unit.
 *
 * Everything here sits in the blob span labelled `Beepgen.c`.  The span is
 * the original TU: `GetGain` is LOCAL (`t`) in the object and lives at the
 * top of it, so the file boundary is the author's and not just the layout's.
 *
 * Most of these symbols have no internal caller in the blob -- they are
 * exported API surface (the reverse-edge probe over the no-entry-point
 * bucket, CLAUDE.md's F8320 discussion) -- so most signatures below are
 * derived from the function body alone and say so where they are thin.
 * The exception is the generator itself: `voice_create` calls
 * `beepgen_create`, and that call is where `struct beepgen_config`'s shape
 * comes from (finding F8766).
 */

#ifndef DSPLIB_BEEPGEN_H
#define DSPLIB_BEEPGEN_H

#ifdef __cplusplus
extern "C" {
#endif

/*
 * One queued tone.  `beepgen_start_beep` appends one of these and
 * `beepgen_sample` walks them; 12 bytes each, twenty of them in the object.
 */
struct beepgen_tone {
	float	freq1;		/* +0x00  column/high tone, Hz, as a float */
	float	freq2;		/* +0x04  row/low tone                     */
	int	duration;	/* +0x08  in 1/dur_units_per_sec seconds   */
};

/* (0x11c - 0x2c) / 12.  The object bounds-checks none of it. */
#define BEEPGEN_TONES	20

/*
 * The beep generator, 0x12c bytes -- the size `beepgen_create` allocates.
 *
 * The oscillator is open-loop: two independent phases advanced by
 * BEEPGEN_PHASE_STEP * frequency per sample, summed through the two gains
 * `GetGain` computes.  A tone lasts `duration` samples, counted in
 * `elapsed`; when it expires the next entry of `tone[]` becomes current, and
 * when the last one expires `queued` goes back to zero.
 *
 * Two frequency values are special and both are the DIAL STRING's, arriving
 * through `beepgen_get_freqs`: 0 silences that half (its gain is forced to
 * zero) and -1 is the '!' start marker, which additionally switches
 * `dur_units_per_sec` from 10 to 100 and fires a callback.
 */
struct beepgen {
	void	*modem;		/* +0x00  handle for modem_get_param       */
	float	phase1;		/* +0x04  oscillator phase, radians        */
	float	phase2;		/* +0x08                                   */
	float	freq1;		/* +0x0c  current tone, Hz                 */
	float	freq2;		/* +0x10                                   */
	float	gain1;		/* +0x14  amplitude of the freq1 tone      */
	float	gain2;		/* +0x18  amplitude of the freq2 tone      */
	int	elapsed;	/* +0x1c  samples emitted from this tone   */
	int	duration;	/* +0x20  samples this tone lasts          */
	int	queued;		/* +0x24  entries in tone[]                */
	int	playing;	/* +0x28  index of the tone being played   */
	struct beepgen_tone tone[BEEPGEN_TONES];
				/* +0x2c                                   */
	/*
	 * Three host callbacks, copied out of `struct beepgen_config`.  The
	 * middle one is named from the object's own debug line: `beepgen_
	 * sample` prints "Hook on proc" immediately before calling it, at
	 * the only site that does.  The other two are neutral names --
	 * `fn_011c` fires when a -1 (start-marker) tone BECOMES CURRENT,
	 * which is the symmetric position, but nothing in the object says
	 * what it is for.
	 *
	 * `fn_0124` SUPPLIES THE MARKER'S DURATION AND ITS 24 IS NO LONGER
	 * UNEXPLAINED.  `voice_create` puts the voice config's S-register
	 * getter in this slot (finding F8813), so the call is
	 * `vce_get_sreg(modem, SREG_FLASH_TIMER)` -- 24 is that register's
	 * number in dsplib/vce.h and in slmodemd's own `modem_defs.h`, and
	 * the answer is VCE_FLASH_TIMER.  Finding F8814.
	 */
	void	(*fn_011c)(void *modem);		/* +0x11c */
	void	(*hook_on_proc)(void *modem);		/* +0x120 */
	int	(*fn_0124)(void *modem, int what);	/* +0x124 */
	int	dur_units_per_sec;
				/* +0x128 10 or 100: `duration` is
				 *        tone.duration * 8000 / this, and
				 *        8000 is the sample rate, so this
				 *        is duration units per second     */
};

/*
 * What `beepgen_create` reads.  Sixteen bytes.
 *
 * THIS PARAGRAPH USED TO SAY `voice_create` PASSES ITS OWN FIRST ARGUMENT
 * THROUGH UNCHANGED, AND IT DOES NOT.  With `voice_create` written
 * (src/service/voicesvc.c) the object is explicit: it builds a SEPARATE
 * 16-byte local and rotates three of the four words into it, so
 * `struct voice_config` (dsplib/voice.h) and this type are two different
 * types that share a size.  In particular THIS `fn_04` is not the voice
 * config's -- it takes the voice config's `fn_08` -- and the S-register
 * getter lands in `fn_0c` below, which is why that slot is the one with a
 * two-argument signature.  Findings F8813 and F8814.
 */
struct beepgen_config {
	void	*modem;			/* +0x00 */
	void	(*fn_04)(void *modem);	/* +0x04 -> beepgen.fn_011c      */
	void	(*fn_08)(void *modem);	/* +0x08 -> beepgen.hook_on_proc */
	int	(*fn_0c)(void *modem, int what);
					/* +0x0c -> beepgen.fn_0124      */
};

/*
 * Radians per sample per hertz.  The object's literal is 0.000785, which is
 * 2*pi/8000 rounded to three figures and NOT the exact value -- so it is
 * kept as written; at 1633 Hz it is 0.05% flat.
 */
#define BEEPGEN_PHASE_STEP	0.000785

/**
 * @brief Build a beep generator.
 * @param bg   NULL allocates; otherwise caller-owned storage.
 * @param cfg  Configuration, copied, not retained.
 * @return @p bg, or the newly allocated generator, or NULL if allocation
 *         failed.
 */
struct beepgen *beepgen_create(struct beepgen *bg,
			       const struct beepgen_config *cfg);

/**
 * @brief Free a generator. A bare `sysdep_free`; NULL is accepted.
 * @param bg  The generator to free.
 */
void beepgen_delete(struct beepgen *bg);

/**
 * @brief Append one tone to the queue.
 *
 * When the queue was EMPTY this also makes the tone current, which is
 * where the gains, the -1 marker and the sample count are settled; when
 * it was not, only the queue entry is written.
 *
 * @param bg        The generator.
 * @param freq1     Column/high tone, Hz (or a special value, see the
 *                  struct comment).
 * @param freq2     Row/low tone, Hz.
 * @param duration  In 1/10 s for an ordinary tone (see `dur_units_per_sec`).
 */
void beepgen_start_beep(struct beepgen *bg, int freq1, int freq2,
			int duration);

/**
 * @brief Queue one dial-string character as DTMF.
 *
 * '!' takes its duration from the config's third callback instead of
 * @p duration, ',' is a pause (silence at three times @p duration), and
 * everything else is the DTMF pair beepgen_get_freqs() gives, appended
 * through beepgen_start_beep().
 *
 * @param bg        The generator.
 * @param code      The dial-string character.
 * @param duration  Tone duration; see beepgen_start_beep().
 */
void beepgen_start_dtmf(struct beepgen *bg, int code, int duration);

/**
 * @brief Generate one 8 kHz sample.
 * @param bg   The generator, advanced by one sample.
 * @param out  Set to the generated sample.
 * @return 1 on the sample that retires the LAST queued tone (and leaves
 *         the queue empty), 0 otherwise.
 */
int beepgen_sample(struct beepgen *bg, float *out);

/**
 * @brief Look up the DTMF frequency pair for one dial character.
 *
 * Unknown characters in '!'..'D' other than the DTMF set fall back to
 * '1' (1209/697), as does anything outside that range; '!' is a "start"
 * marker and yields -1/-1.
 *
 * @param code  The dial character.
 * @param colp  Set to the column tone (1209/1336/1477/1633 Hz).
 * @param rowp  Set to the row tone (697/770/852/941 Hz).
 */
void beepgen_get_freqs(unsigned char code, int *colp, int *rowp);

/**
 * @brief Compute the two DTMF tone gains from the modem's level parameters.
 *
 * `gain1`/`gain2` are the object's own words: its debug lines print
 * "BeepGen: GAIN1*1000" for @p gain1 and "GAIN2*1000" for @p gain2.
 * LOCAL in the blob, so GCC 3.4 gave it `regparm(2)` there; it is `static`
 * in Beepgen.c now so our copy takes the building compiler's static
 * convention, and the differential test declares that convention and
 * reaches it through the globalized test copy (tools/testvisible.py).
 *
 * @param bg     The generator, for its `modem` handle.
 * @param gain1  Set to `pow(10, -GetDTMFHighAndLowToneLevelDifference * 0.05) * *gain2`.
 * @param gain2  Set to `pow(10, (6 - GetDTMFHighToneLevel) * 0.05 - GetAdditAttenToBeepgenVoice * 0.05) * 0.276`.
 */

/**
 * @brief Majority check over a window of shorts, against detector history.
 * @param w  Window; `w[0]` must equal `w[1]` and `w[2]`, and differ from
 *           each of `w[3]..w[6]`.
 * @return 1 when the shape holds, 0 otherwise.
 */
int check_for_valid(unsigned short *w);

/**
 * @brief Relaxed form of check_for_valid().
 * @param w  Window; `w[0]` must equal `w[1]` and differ from `w[2]` and
 *           `w[3]`.
 * @return 1 when the shape holds, 0 otherwise.
 */
int check_for_valid_easy(unsigned short *w);

/**
 * @brief Convert float samples to 16-bit linear, with a gain.
 *
 * A no-op when @p gain is exactly 0.0f -- the object tests equality, not
 * magnitude. Truncates toward zero (the object sets the x87
 * round-to-zero bits around its `fistp`), which is the C cast.
 *
 * @param src   Input float samples.
 * @param dst   Output linear samples.
 * @param n     Number of samples.
 * @param gain  Applied before conversion.
 */
void zFLTUTL_Float2Linear(float *src, short *dst, int n, float gain);

/**
 * @brief Convert 16-bit linear samples to float, with a gain.
 *
 * A no-op when @p gain is exactly 0.0f.
 *
 * @param src   Input linear samples.
 * @param dst   Output float samples.
 * @param n     Number of samples.
 * @param gain  Applied after conversion.
 */
void zFLTUTL_Linear2Float(short *src, float *dst, int n, float gain);

/**
 * @brief Mean-removed average power of a float buffer (called "RMS" by
 * the object; no square root is taken).
 * @param n    Number of samples.
 * @param buf  Samples.
 * @return The mean-removed average power.
 */
float fComputeRMSValueFloatBuf(unsigned int n, float *buf);

/**
 * @brief Mean-removed average power of a short buffer.
 *
 * The mean is an INTEGER here (`sum/n` in unsigned integer division),
 * unlike the float flavour -- both details are the object's.
 *
 * @param n    Number of samples.
 * @param buf  Samples.
 * @return The mean-removed average power.
 */
float fComputeRMSValueShortBuf(unsigned int n, short *buf);

/**
 * @brief Convert a linear/float pair each way at once, at fixed
 * +/-32000 full scale.
 * @param lin_in   Linear input, converted to float.
 * @param flt_out  `flt_out[i] = lin_in[i] / 32000`.
 * @param flt_in   Float input, converted to linear.
 * @param lin_out  `lin_out[i] = (short)(flt_in[i] * 32000)`, truncated.
 * @param n        Number of samples in each direction.
 */
void CrossDataLinks(short *lin_in, float *flt_out, float *flt_in,
		    short *lin_out, int n);

/**
 * @brief Slide two parallel windows and test their trailing energy against
 * a fixed threshold.
 *
 * Slides both windows left by @p fresh of @p total samples, appends the
 * two fresh blocks, and tests the mean-removed energy (/1000) of
 * `buf1[1000..1999]` against 40000.0f.
 *
 * @param new1   Fresh block to append to @p buf1.
 * @param new2   Fresh block to append to @p buf2.
 * @param buf1   Sliding window, updated in place.
 * @param buf2   Sliding window, updated in place.
 * @param fresh  Number of fresh samples.
 * @param total  Total window length.
 * @return 1 only when `buf2[1] != 0` and the energy exceeds the
 *         threshold, 0 otherwise.
 */
int bSearchEnergy(short *new1, short *new2, short *buf1, short *buf2,
		  unsigned int fresh, unsigned int total);

/**
 * @brief Cross-correlate a signal against a pattern over a range of lags.
 *
 * Cross-correlates `sig[*posp..]` against a 1000-sample @p pattern for
 * up to 20 lags, stopping 1000 samples short of @p len.
 *
 * @param pattern  1000-sample reference pattern.
 * @param sig      Signal to search.
 * @param posp     In/out: starting position, advanced to the last lag examined.
 * @param len      Length of @p sig.
 * @param peakp    In/out: running peak `|correlation|`.
 * @param peakposp In/out: lag of the running peak.
 * @param out      Each correlation (scaled by 1e-4), written as a
 *                 truncated short.
 * @return 1 when there was nothing left to scan, 0 otherwise.
 */
int FindCorrelation(short *pattern, short *sig, unsigned int *posp,
		    unsigned int len, unsigned int *peakp,
		    unsigned int *peakposp, short *out);

/**
 * @brief Maximum absolute value over a float buffer.
 * @param buf  Samples; `buf[0]` unconditionally seeds the maximum.
 * @param n    Number of samples.
 * @return max(|buf[i]|).
 */
float zfFLTUTL_GetMaxAbsValue(float *buf, unsigned int n);

/**
 * @brief The datapump wrapper's per-block linear/float conversion.
 *
 * Sits between FindCorrelation() and zfFLTUTL_GetMaxAbsValue() in the
 * object (0xae490) and so is inside this file's address range even
 * though its name belongs with Fdspkrnl.c; declared here for that
 * reason, to move when FDSP_DP_Create lands.
 *
 * Converts `*countp` samples each way and does nothing else: the
 * receive side is 16-bit linear scaled by 1/32000, the transmit side
 * float scaled by 32000 and truncated toward zero.
 *
 * @param status     Set to 2.
 * @param rx_lin     Receive side, linear input.
 * @param rx_flt     Receive side, float output.
 * @param tx_flt     Transmit side, float input.
 * @param tx_lin     Transmit side, linear output.
 * @param hostcount  READ BY NOTHING HERE -- this function never loads
 *                   that stack slot. A sibling types it: `voice_online`
 *                   (voice.h) has this signature slot for slot and
 *                   WRITES it as `unsigned short *`, twice; `voice_duplex`
 *                   forwards its own such argument straight into this
 *                   call. Finding F8786, deviation D986.
 * @param countp     Number of samples to convert each way.
 * @return 1, unconditionally.
 */
int FDSP_DP_Run(int *status, short *rx_lin, float *rx_flt, float *tx_flt,
		short *tx_lin, unsigned short *hostcount,
		unsigned short *countp);

#ifdef __cplusplus
}
#endif

#endif /* DSPLIB_BEEPGEN_H */

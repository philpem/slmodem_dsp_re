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
	 * what it is for; `fn_0124` supplies the marker's duration and its
	 * 24 is unexplained (see beepgen_start_dtmf).
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
 * What `beepgen_create` reads.  Sixteen bytes, and `voice_create` (not
 * reconstructed) passes its OWN first argument through unchanged, so this
 * is the voice service's configuration seen from the beep generator's end.
 * If a voice reconstruction needs the same block it must include this
 * header rather than spell the type again.
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

/*
 * `cfg` is copied, not retained.  A NULL `bg` allocates; the result is NULL
 * only when that allocation fails.
 */
struct beepgen *beepgen_create(struct beepgen *bg,
			       const struct beepgen_config *cfg);

/* Frees whatever it is given, NULL included -- it is a bare sysdep_free. */
void beepgen_delete(struct beepgen *bg);

/*
 * Append one tone.  When the queue was EMPTY this also makes the tone
 * current, which is where the gains, the -1 marker and the sample count are
 * settled; when it was not, only the queue entry is written.  `duration` is
 * in 1/10 s for an ordinary tone (see dur_units_per_sec).
 */
void beepgen_start_beep(struct beepgen *bg, int freq1, int freq2,
			int duration);

/*
 * One dial-string character.  '!' takes its duration from the config's
 * third callback instead of the argument, ',' is a pause -- silence at
 * three times the duration -- and everything else is the DTMF pair
 * `beepgen_get_freqs` gives, appended through `beepgen_start_beep`.
 */
void beepgen_start_dtmf(struct beepgen *bg, int code, int duration);

/*
 * One 8 kHz sample into *out.  Returns 1 on the sample that retires the
 * LAST queued tone (and leaves the queue empty), 0 otherwise.
 */
int beepgen_sample(struct beepgen *bg, float *out);

/*
 * DTMF frequencies for one dial character.  `*colp` gets the column tone
 * (1209/1336/1477/1633 Hz), `*rowp` the row tone (697/770/852/941 Hz).
 * Unknown characters in '!'..'D' other than the DTMF set fall back to '1'
 * (1209/697), as does anything outside that range; '!' is a "start" marker
 * and yields -1/-1.
 */
void beepgen_get_freqs(unsigned char code, int *colp, int *rowp);

/*
 * Two gains from three modem parameters:
 *
 *   *gain2 = pow(10, (6 - GetDTMFHighToneLevel) * 0.05
 *                    - GetAdditAttenToBeepgenVoice * 0.05) * 0.276
 *   *gain1 = pow(10, -GetDTMFHighAndLowToneLevelDifference * 0.05) * *gain2
 *
 * gain1/gain2 are the object's own words: its debug lines print
 * "BeepGen: GAIN1*1000" for the pointer passed second and "GAIN2*1000" for
 * the pointer passed third.
 *
 * LOCAL in the blob, so GCC 3.4 gave it regparm(2) there; our copy has
 * external linkage and the ordinary convention, the same trade
 * t_dialstring.c documents for AnalyseDialString.
 */
void GetGain(struct beepgen *bg, float *gain1, float *gain2);

/*
 * Majority checks over a window of shorts, used against detector history.
 * check_for_valid: w[0] must equal w[1] and w[2], and differ from each of
 * w[3]..w[6].  check_for_valid_easy: w[0] must equal w[1] and differ from
 * w[2] and w[3].  1 when the shape holds, 0 otherwise.
 */
int check_for_valid(unsigned short *w);
int check_for_valid_easy(unsigned short *w);

/*
 * Linear (16-bit) <-> float conversion with a gain.  Both are no-ops when
 * the gain is exactly 0.0f -- the object tests equality, not magnitude.
 * Float2Linear truncates toward zero (the object sets the x87 round-to-zero
 * bits around its fistp), which is the C cast.
 */
void zFLTUTL_Float2Linear(float *src, short *dst, int n, float gain);
void zFLTUTL_Linear2Float(short *src, float *dst, int n, float gain);

/*
 * Mean-removed average power (the "RMS" of the name is the object's; no
 * square root is taken).  The mean is an INTEGER for the short flavour --
 * sum/n in unsigned integer division -- and a float for the float flavour,
 * which also multiplies by a reciprocal at the end where the short flavour
 * divides.  Both details are the object's.
 */
float fComputeRMSValueFloatBuf(unsigned int n, float *buf);
float fComputeRMSValueShortBuf(unsigned int n, short *buf);

/*
 * Both conversions at once, at the fixed +/-32000 full scale:
 * fout[i] = sin[i] / 32000, sout[i] = (short)(fin[i] * 32000) truncated.
 */
void CrossDataLinks(short *lin_in, float *flt_out, float *flt_in,
		    short *lin_out, int n);

/*
 * Slide two parallel short windows left by `fresh` of `total` samples,
 * append the two fresh blocks, and test the energy (mean-removed, /1000) of
 * buf1[1000..1999] against 40000.0f.  Returns 1 only when buf2[1] != 0 and
 * the energy exceeds the threshold.
 */
int bSearchEnergy(short *new1, short *new2, short *buf1, short *buf2,
		  unsigned int fresh, unsigned int total);

/*
 * Cross-correlate sig[pos..] against a 1000-sample pattern for up to 20
 * lags, stopping 1000 samples short of `len`.  Each correlation (scaled by
 * 1e-4) is written to out[] as a truncated short; the running peak
 * |correlation| and its lag are maintained through *peakp / *peakposp.
 * *posp advances to the last lag examined.  Returns 1 when there was
 * nothing left to scan, 0 otherwise.
 */
int FindCorrelation(short *pattern, short *sig, unsigned int *posp,
		    unsigned int len, unsigned int *peakp,
		    unsigned int *peakposp, short *out);

/* max |buf[i]| over n entries; buf[0] unconditionally seeds the maximum. */
float zfFLTUTL_GetMaxAbsValue(float *buf, unsigned int n);

/*
 * FDSP_DP_Run -- the datapump wrapper's per-block conversion, 0xae490, which
 * sits between FindCorrelation and zfFLTUTL_GetMaxAbsValue and so is inside
 * this file's address range even though its name belongs with fdspkrnl.c.
 * Declared here for that reason; move it when FDSP_DP_Create lands.
 *
 * It converts `*countp` samples each way and does nothing else: the receive
 * side is 16-bit linear scaled by 1/32000, the transmit side float scaled by
 * 32000 and truncated toward zero.  `*status` is set to 2 and 1 is returned
 * unconditionally.  The sixth argument is READ BY NOTHING -- it occupies a
 * stack slot the object never loads -- so its type here is a placeholder.
 */
int FDSP_DP_Run(int *status, short *rx_lin, float *rx_flt, float *tx_flt,
		short *tx_lin, void *unused, unsigned short *countp);

#ifdef __cplusplus
}
#endif

#endif /* DSPLIB_BEEPGEN_H */

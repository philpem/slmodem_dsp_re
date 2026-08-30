/*
 * beepgen.h -- the beep generator's DTMF frequency table and the float/linear
 * utility set that shares its translation unit.
 *
 * Everything here sits in the blob span labelled `Beepgen.c`.  The span is
 * the original TU: `GetGain` is LOCAL (`t`) in the object and lives at the
 * top of it, so the file boundary is the author's and not just the layout's.
 *
 * None of these symbols has an internal caller in the blob -- they are
 * exported API surface (the reverse-edge probe over the no-entry-point
 * bucket, CLAUDE.md's F8320 discussion) -- so every signature below is
 * derived from the function body alone and says so where it is thin.
 */

#ifndef DSPLIB_BEEPGEN_H
#define DSPLIB_BEEPGEN_H

#ifdef __cplusplus
extern "C" {
#endif

/*
 * The beep generator object, as far as `GetGain` sees it: one pointer, at
 * +0, handed to modem_get_param.  The real object is certainly larger --
 * nothing reconstructed yet allocates one -- so this models only the prefix
 * that is established.  Extend it from the creator's disassembly when that
 * is written, not from here.
 */
struct beepgen {
	void	*modem;		/* +0x00  handle for modem_get_param */
};

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

#ifdef __cplusplus
}
#endif

#endif /* DSPLIB_BEEPGEN_H */

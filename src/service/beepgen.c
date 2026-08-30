/*
 * beepgen.c -- the beep generator's frequency/gain queries and the
 * float/linear utility set that shares the blob span labelled `Beepgen.c`.
 *
 * Functions appear in the object's emission order (GetGain 0xac960 ..
 * zfFLTUTL_GetMaxAbsValue 0xae850).  Symbols with other addresses in
 * between belong to other functions of the same TU, not yet reconstructed.
 *
 * None of these has an internal caller in the blob; every claim about a
 * signature is derived from the body and the differential tests, and the
 * header says where that is thin.
 */

#include <math.h>

#include "dsplib/beepgen.h"
#include "dsplib/debug.h"
#include "dsplib/modem_params.h"
#include "dsplib/sysdep.h"

/*
 * (x) > 0 ? (x) : -(x), spelled as the object spells it: a compare against
 * +0.0 and a conditional negate, re-evaluated at each use exactly as a
 * macro argument would be (zfFLTUTL_GetMaxAbsValue re-tests the sign after
 * taking the maximum, which is the macro's second expansion).
 */
#define BEEP_FABS(x) ((x) > 0.0f ? (x) : -(x))

/*
 * Two gains from three modem parameters; the debug lines' GAIN1/GAIN2 are
 * the object's names for the two outputs.  0.05 is dB-to-log10 at 20dB per
 * decade; 0.276 is an unexplained base scale, kept as the literal.
 *
 * LOCAL in the blob (so regparm(2) there); external linkage and the
 * ordinary convention here -- each side is called as its own compiler
 * built it.
 */
void
GetGain(struct beepgen *bg, float *gain1, float *gain2)
{
	int high = (int)modem_get_param(bg->modem, GetDTMFHighToneLevel);
	int diff = (int)modem_get_param(bg->modem,
					GetDTMFHighAndLowToneLevelDifference);
	double base = (double)(6 - high) * 0.05;
	int atten = (int)modem_get_param(bg->modem,
					 GetAdditAttenToBeepgenVoice);

	*gain2 = pow(10.0, (double)atten * -0.05 + base) * 0.276;
	*gain1 = pow(10.0, (double)-diff * 0.05) * *gain2;

	if (DSPLIB_DEBUG_ON())
		dsplibs_debug_printf(
		    "BeepGen: Additional Attenuation to Beepgen Voice = %d.\n",
		    (int)modem_get_param(bg->modem,
					 GetAdditAttenToBeepgenVoice));
	if (DSPLIB_DEBUG_ON())
		dsplibs_debug_printf("BeepGen: GAIN1*1000 = %d.\n",
				     (int)(*gain1 * 1000.0f));
	if (DSPLIB_DEBUG_ON())
		dsplibs_debug_printf("BeepGen: GAIN2*1000 = %d.\n",
				     (int)(1000.0f * *gain2));
}

/*
 * DTMF pair for one dial character.  Column and row tables carry two spare
 * entries -- a 0 at [4] no case reaches and a -1 at [5] for the '!' start
 * marker -- exactly as the object initialises them.
 */
void
beepgen_get_freqs(unsigned char code, int *colp, int *rowp)
{
	int col[6] = { 1209, 1336, 1477, 1633, 0, -1 };
	int row[6] = { 697, 770, 852, 941, 0, -1 };
	int ri = 0, ci = 0;

	switch (code) {
	case '!':
		/* The object's own marker for the start of a dial string. */
		if (DSPLIB_DEBUG_ON())
			dsplibs_debug_printf(
			    "start !!!!!!!!!!!!!!!!!!!!!!!!\n");
		ri = 5; ci = 5;
		break;
	case '1': ri = 0; ci = 0; break;
	case '2': ri = 0; ci = 1; break;
	case '3': ri = 0; ci = 2; break;
	case 'A': ri = 0; ci = 3; break;
	case '4': ri = 1; ci = 0; break;
	case '5': ri = 1; ci = 1; break;
	case '6': ri = 1; ci = 2; break;
	case 'B': ri = 1; ci = 3; break;
	case '7': ri = 2; ci = 0; break;
	case '8': ri = 2; ci = 1; break;
	case '9': ri = 2; ci = 2; break;
	case 'C': ri = 2; ci = 3; break;
	case '*': ri = 3; ci = 0; break;
	case '0': ri = 3; ci = 1; break;
	case '#': ri = 3; ci = 2; break;
	case 'D': ri = 3; ci = 3; break;
	default:  ri = 0; ci = 0; break;
	}
	*colp = col[ci];
	*rowp = row[ri];
}

/*
 * w[0] must repeat through w[2] and then NOT reappear in w[3]..w[6].
 * A stability check over a detector history window.
 */
int
check_for_valid(unsigned short *w)
{
	unsigned short v = w[0];

	if (v != w[1])
		return 0;
	if (v != w[2])
		return 0;
	if (v == w[3])
		return 0;
	if (v == w[4])
		return 0;
	if (v == w[5])
		return 0;
	if (v == w[6])
		return 0;
	return 1;
}

/* The short window: w[0] == w[1], and w[0] absent from w[2]..w[3]. */
int
check_for_valid_easy(unsigned short *w)
{
	unsigned short v = w[0];

	if (v != w[1])
		return 0;
	if (v == w[2])
		return 0;
	if (v == w[3])
		return 0;
	return 1;
}

/*
 * float -> 16-bit linear at a gain; the (short) cast truncates toward zero
 * (the object programs the x87 round-to-zero bits around its store).  A
 * gain of exactly 0.0f is "off" and leaves dst untouched.
 */
void
zFLTUTL_Float2Linear(float *src, short *dst, int n, float gain)
{
	int i;

	if (gain == 0.0f)
		return;
	for (i = 0; i < n; i++)
		dst[i] = (short)(src[i] * gain);
}

/* 16-bit linear -> float at a gain, same 0.0f-is-off convention. */
void
zFLTUTL_Linear2Float(short *src, float *dst, int n, float gain)
{
	int i;

	if (gain == 0.0f)
		return;
	for (i = 0; i < n; i++)
		dst[i] = src[i] * gain;
}

/*
 * Mean-removed average power over a float buffer.  The tail multiplies by
 * 1/n where the short flavour divides -- both are the object's spellings.
 */
float
fComputeRMSValueFloatBuf(unsigned int n, float *buf)
{
	float sum = 0.0f, acc = 0.0f, mean;
	unsigned int i;

	for (i = 0; i < n; i++)
		sum += buf[i];
	mean = sum / (float)n;
	for (i = 0; i < n; i++) {
		float d = buf[i] - mean;

		acc += d * d;
	}
	return acc * (1.0f / (float)n);
}

/*
 * Same shape over shorts, with an INTEGER mean: sum/n in unsigned integer
 * division, so the removed mean is floor-ish and the residual is what the
 * object computes, not what a float mean would give.
 */
float
fComputeRMSValueShortBuf(unsigned int n, short *buf)
{
	unsigned int sum = 0, i;
	int mean;
	float acc = 0.0f;

	for (i = 0; i < n; i++)
		sum += buf[i];
	mean = sum / n;
	for (i = 0; i < n; i++) {
		float d = (float)(buf[i] - mean);

		acc += d * d;
	}
	return acc / (float)n;
}

/*
 * Both conversions at once at the +/-32000 full scale used across the
 * voice path.  The truncating cast and constant handling match
 * zFLTUTL_Float2Linear's.
 */
void
CrossDataLinks(short *lin_in, float *flt_out, float *flt_in, short *lin_out,
	       int n)
{
	int i;

	for (i = 0; i < n; i++)
		flt_out[i] = lin_in[i] * (1.0f / 32000.0f);
	for (i = 0; i < n; i++)
		lin_out[i] = (short)(flt_in[i] * 32000.0f);
}

/*
 * Slide the two parallel windows left by `fresh`, append the fresh blocks,
 * and grade the energy of buf1[1000..1999].  buf2[1] gates the verdict --
 * with it zero the energy is computed and discarded, which is faithful.
 */
int
bSearchEnergy(short *new1, short *new2, short *buf1, short *buf2,
	      unsigned int fresh, unsigned int total)
{
	unsigned int keep = total - fresh, i;
	unsigned int sum = 0;
	int mean, ret = 0;
	float acc = 0.0f;

	for (i = 0; i < keep; i++)
		buf1[i] = buf1[i + fresh];
	for (i = 0; i < keep; i++)
		buf2[i] = buf2[i + fresh];
	sysdep_memcpy(buf1 + keep, new1, 2 * fresh);
	sysdep_memcpy(buf2 + keep, new2, 2 * fresh);

	for (i = 0; i < 1000; i++)
		sum += buf1[1000 + i];
	mean = sum / 1000;
	for (i = 0; i < 1000; i++) {
		float d = (float)(buf1[1000 + i] - mean);

		acc += d * d;
	}
	if (buf2[1] != 0) {
		if (acc * 0.001f > 40000.0f)
			ret = 1;
	}
	return ret;
}

/*
 * Correlate up to 20 lags of sig against a 1000-sample pattern.  corrbuf is
 * the object's: written every lag, read by nothing -- likely a debug
 * leftover, kept because dropping it would drop its stores.
 */
int
FindCorrelation(short *pattern, short *sig, unsigned int *posp,
		unsigned int len, unsigned int *peakp,
		unsigned int *peakposp, short *out)
{
	unsigned int pos = *posp;
	unsigned int end = len - 1000;
	unsigned int i, k;
	int ret = 1;
	float corrbuf[20];

	if (end > pos + 20)
		end = pos + 20;
	if (pos != end) {
		ret = 0;
		k = 0;
		for (i = pos; i < end; i++, k++) {
			float acc = 0.0f, corr;
			unsigned int j, a;

			for (j = 0; j <= 999; j++)
				acc += sig[i + j] * pattern[j];
			corr = acc * 0.0001;
			corrbuf[k] = corr;
			out[k] = (short)corr;
			a = (unsigned int)fabs(corr);
			if (*peakp < a) {
				*peakp = a;
				*peakposp = i;
			}
		}
	}
	*posp = end;
	return ret;
}

/*
 * Maximum |buf[i]|.  BEEP_FABS is expanded twice per update exactly as the
 * object re-tests the sign after copying the winner, so the macro is the
 * source shape and not a convenience.
 */
float
zfFLTUTL_GetMaxAbsValue(float *buf, unsigned int n)
{
	float max = BEEP_FABS(buf[0]);
	unsigned int i;

	for (i = 1; i < n; i++) {
		if (BEEP_FABS(buf[i]) > max)
			max = BEEP_FABS(buf[i]);
	}
	return max;
}

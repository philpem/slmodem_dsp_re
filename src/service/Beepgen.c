/*
 * Beepgen.c -- the beep generator's frequency/gain queries and the
 * float/linear utility set that shares the blob span labelled `Beepgen.c`.
 *
 * Functions appear in the object's emission order (GetGain 0xac960 ..
 * zfFLTUTL_GetMaxAbsValue 0xae850).  Symbols with other addresses in
 * between belong to other functions of the same TU, not yet reconstructed.
 *
 * MOST of these have no internal caller in the blob and their signatures are
 * derived from the body alone -- but not all, and finding F8766 is the
 * correction: `voice_create` calls `beepgen_create` (0xac2d5), which is what
 * types the sixteen-byte configuration block, `detector_create` calls
 * `create_dtmf`, and six sites call `detector_set_enable`. The header says
 * where a signature is still thin.
 */

#include <math.h>

#include "dsplib/beepgen.h"
#include "dsplib/cadence.h"
#include "dsplib/debug.h"
#include "dsplib/detector.h"
#include "dsplib/dtmf.h"
#include "dsplib/fdspkrnl.h"
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
 * LOCAL in the blob (so regparm(2) there); `static` here so the building
 * compiler gives our copy its own static convention.  The differential test
 * declares that convention and reaches the function through the globalized
 * test copy (tools/testvisible.py).
 */
static void
GetGain(struct beepgen *bg, float *gain1, float *gain2)
{
	int high = (int)modem_get_param(bg->modem, GetDTMFHighToneLevel);
	int diff = (int)modem_get_param(bg->modem,
					GetDTMFHighAndLowToneLevelDifference);
	double base = (double)(6 - high) * 0.05;
	int atten = (int)modem_get_param(bg->modem,
					 GetAdditAttenToBeepgenVoice);

	*gain2 = (float)(pow(10.0, (double)atten * -0.05 + base) * 0.276);
	*gain1 = (float)(pow(10.0, (double)-diff * 0.05) * *gain2);

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
 * Allocate when asked, then clear everything the generator reads.  The
 * config block is COPIED -- the object keeps no pointer to it -- and the
 * "%X  %X" line prints the two callbacks it just stored.
 */
struct beepgen *
beepgen_create(struct beepgen *bg, const struct beepgen_config *cfg)
{
	if (DSPLIB_DEBUG_ON())
		dsplibs_debug_printf("beepgen_create\n");

	if (bg == NULL) {
		bg = sysdep_malloc(sizeof(struct beepgen));
		if (bg == NULL)
			return NULL;
	}

	bg->freq1 = 0.0f;
	bg->freq2 = 0.0f;
	bg->modem = cfg->modem;
	bg->phase1 = 0.0f;
	bg->phase2 = 0.0f;
	/*
	 * This call site's argument order is the OPPOSITE of the two that
	 * start a tone, so a freshly created object carries the pair the
	 * wrong way round until the first beep recomputes them.  It is the
	 * object's, at 0xacd2f; see D987.
	 */
	GetGain(bg, &bg->gain1, &bg->gain2);
	bg->queued = 0;
	bg->playing = 0;
	bg->hook_on = cfg->hook_on;
	bg->hook_off = cfg->hook_off;
	bg->get_sreg = cfg->get_sreg;

	if (DSPLIB_DEBUG_ON())
		dsplibs_debug_printf("%X  %X\n", bg->hook_on,
				     bg->hook_off);
	return bg;
}

/* A bare tail call to the allocator's free; NULL is the allocator's problem. */
void
beepgen_delete(struct beepgen *bg)
{
	sysdep_free(bg);
}

/*
 * Queue one tone.  Everything inside the `queued == 0` arm is the work of
 * making the tone CURRENT -- the gains, the two special frequencies, and
 * the sample count -- and is skipped when something is already playing,
 * because beepgen_sample does the same work when it advances.
 *
 * The gain stores in the special arms are of an integer zero in the object;
 * 0.0f has that bit pattern, so this is a float assignment either way.
 */
void
beepgen_start_beep(struct beepgen *bg, int freq1, int freq2, int duration)
{
	struct beepgen_tone *t;

	if (DSPLIB_DEBUG_ON())
		dsplibs_debug_printf("start beep %d %d %d\n", freq1, freq2,
				     duration);

	if (bg->queued == 0) {
		bg->dur_units_per_sec = 10;
		bg->freq1 = (float)freq1;
		bg->freq2 = (float)freq2;
		/*
		 * THE ARGUMENTS ARE THE OTHER WAY ROUND FROM beepgen_create'S,
		 * and that is the object's (0xacea4 against 0xacd2f, under
		 * GetGain's regparm(2)).  This order is the sensible one:
		 * GetGain's third output is the unattenuated level and it
		 * lands on freq1, the DTMF column, which is the high group.
		 * See D987.
		 */
		GetGain(bg, &bg->gain2, &bg->gain1);
		if (freq1 == 0) {
			bg->gain1 = 0.0f;
		} else if (freq1 == -1) {
			bg->dur_units_per_sec = 100;
			bg->gain1 = 0.0f;
			if (bg->hook_on != NULL)
				bg->hook_on(bg->modem);
		}
		if (freq2 == 0)
			bg->gain2 = 0.0f;
		bg->elapsed = 0;
		bg->playing = 0;
		bg->duration = duration * 8000 / bg->dur_units_per_sec;
	}

	t = &bg->tone[bg->queued];
	t->duration = duration;
	t->freq1 = (float)freq1;
	t->freq2 = (float)freq2;
	bg->queued++;
}

/*
 * One dial-string character.  The two characters that are not a DTMF pair:
 *
 *   '!'  the start marker.  Its duration comes from the config's third
 *        callback rather than the argument -- the 24 it is passed is the
 *        object's literal and is not explained anywhere in it -- and
 *        beepgen_get_freqs gives it -1/-1, which beepgen_start_beep reads
 *        as the marker.
 *   ','  a pause: silence, at three times the duration.
 */
void
beepgen_start_dtmf(struct beepgen *bg, int code, int duration)
{
	int col, row;

	if (code == '!')
		duration = bg->get_sreg(bg->modem, 24);

	if (DSPLIB_DEBUG_ON())
		dsplibs_debug_printf("beepgen start dtmf %d %d\n", code,
				     duration);

	beepgen_get_freqs((unsigned char)code, &col, &row);

	if (code == ',') {
		if (DSPLIB_DEBUG_ON())
			dsplibs_debug_printf("\n *** digit ',' => pause");
		col = 0;
		row = 0;
		duration = duration * 3;
	}

	beepgen_start_beep(bg, col, row, duration);
}

/*
 * One 8 kHz sample, then the end-of-tone bookkeeping.
 *
 * THE TWO TRIGONOMETRIC CALLS ARE `fsin` AND `fcos` IN THE OBJECT (0xad2a8
 * and 0xad2af) and that is a build-flag observation, not a source one: GCC
 * expands sin()/cos() inline to the x87 instructions only under
 * -funsafe-math-optimizations, which src/dsp/fft.cpp records the same object
 * needing for its twiddle angles.  The pragma below is scoped to this
 * function so the rest of the file keeps the tree's ordinary flags; GCC
 * 3.4.2 ignores it entirely and calls libm, which differs from `fsin` around
 * the 54th bit of a quantity that is then rounded to a 24-bit float.  See
 * fft.cpp's header for why that is invisible to the differential tier and
 * still worth spelling correctly.
 *
 * The whole sample expression is evaluated in double (x87 extended): the
 * object multiplies the 80-bit sine by the float gain with `fmuls` and only
 * the final sum is narrowed by the store.
 */
#if defined(__GNUC__) && !defined(__cplusplus) && __GNUC__ >= 4
#pragma GCC push_options
#pragma GCC optimize("unsafe-math-optimizations")
#endif
int
beepgen_sample(struct beepgen *bg, float *out)
{
	struct beepgen_tone *t;

	bg->elapsed++;
	*out = (float)(sin(bg->phase1) * bg->gain1 + cos(bg->phase2) * bg->gain2);
	bg->phase1 += (float)(BEEPGEN_PHASE_STEP * bg->freq1);
	bg->phase2 += (float)(BEEPGEN_PHASE_STEP * bg->freq2);
	if (bg->elapsed <= bg->duration)
		return 0;

	if (DSPLIB_DEBUG_ON())
		dsplibs_debug_printf("End of BEEP\n");

	if (bg->freq1 == -1.0f) {
		if (DSPLIB_DEBUG_ON())
			dsplibs_debug_printf("Hook on proc\n");
		if (bg->hook_off != NULL)
			bg->hook_off(bg->modem);
	}

	bg->playing++;
	if (bg->playing >= bg->queued) {
		bg->queued = 0;
		return 1;
	}

	if (DSPLIB_DEBUG_ON())
		dsplibs_debug_printf("Start beep[%02d]\n", bg->playing);

	bg->dur_units_per_sec = 10;
	bg->freq1 = bg->tone[bg->playing].freq1;
	bg->freq2 = bg->tone[bg->playing].freq2;
	/* Same order as beepgen_start_beep's, not beepgen_create's -- D987. */
	GetGain(bg, &bg->gain2, &bg->gain1);

	t = &bg->tone[bg->playing];
	if (t->freq1 == 0.0f) {
		bg->gain1 = 0.0f;
	} else if (t->freq1 == -1.0f) {
		bg->gain1 = 0.0f;
		bg->dur_units_per_sec = 100;
		if (bg->hook_on != NULL)
			bg->hook_on(bg->modem);
	}
	if (bg->tone[bg->playing].freq2 == 0.0f)
		bg->gain2 = 0.0f;
	bg->elapsed = 0;
	bg->duration = bg->tone[bg->playing].duration * 8000
		       / bg->dur_units_per_sec;
	return 0;
}
#if defined(__GNUC__) && !defined(__cplusplus) && __GNUC__ >= 4
#pragma GCC pop_options
#endif

/*
 * detector_delete, 0xad620.  Tears the object down in the object's own order:
 * the +0x04 block, then all four tones, then whichever cadences exist, then
 * the detector.
 *
 * NEITHER the +0x04 block NOR any of the four tone pointers is guarded, and
 * the three cadences are -- that asymmetry is the object's and is why the
 * loop and the three tests are spelled differently here.  The loop bound is
 * `<= 3` because the object's is `cmp $0x3,%ebx; jle`, on a signed counter.
 */
void
detector_delete(struct detector *d)
{
	int i;

	sysdep_free(d->dtmf);
	for (i = 0; i <= 3; i++)
		TONE_delete(d->tone[i]);
	if (d->cadence_0008 != NULL)
		cadence_delete(d->cadence_0008);
	if (d->cadence_busy != NULL)
		cadence_delete(d->cadence_busy);
	if (d->cadence_dial != NULL)
		cadence_delete(d->cadence_dial);
	sysdep_free(d);
}

/*
 * The three detector setters, 0xad6b0-0xad6d0.  Each is a single store and
 * nothing here reads the value back; see detector.h for what is and is not
 * established about the object.
 */
void
detector_set_enable(struct detector *d, unsigned short enable)
{
	d->enable = enable;
}

void
detector_set_output_status(struct detector *d)
{
	d->output_mode = DETECTOR_OUTPUT_STATUS;
}

void
detector_set_output_in_stream(struct detector *d)
{
	d->output_mode = DETECTOR_OUTPUT_IN_STREAM;
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
 * Allocate (when asked) and initialise one DTMF receiver.  The three loops
 * count in a SHORT -- the object sign-extends the counter with `cwtl` and
 * compares 16 bits at 0xadf28, 0xadf49 and 0xadf68 -- and the bounds are
 * written as the object's `<= 7` and `<= 1` rather than the `< 8` a fresh
 * author would write, because that is what those compares say.
 *
 * Everything not touched here is `pad_80` in struct dtmf: create_dtmf leaves
 * 0x80..0x8f alone.
 */
struct dtmf *
create_dtmf(struct dtmf *d)
{
	short i;

	if (d == NULL) {
		d = sysdep_malloc(sizeof(struct dtmf));
		if (d == NULL) {
			if (DSPLIB_DEBUG_ON())
				dsplibs_debug_printf(
				    "could not allocate DTMF channel\n");
			return NULL;
		}
	}

	for (i = 0; i <= 7; i++) {
		d->notch_state[i][0] = 0.0f;
		d->notch_state[i][1] = 0.0f;
		d->energy[i] = 0.0f;
	}
	for (i = 0; i <= 1; i++)
		d->bias_state[i] = 0.0f;
	for (i = 0; i <= 7; i++)
		d->hist[i] = -1;

	d->total = 0.0f;
	d->phase = 0;
	d->count = 0;
	d->held = 0;
	d->digit = -1;
	d->easy = 0;
	return d;
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
		dst[i] = (float)src[i] * gain;
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
 * Initialise the power accumulator after the integer sum pass: this places
 * the x87 zero at the division, as in the object (finding F10223).
 */
float
fComputeRMSValueShortBuf(unsigned int n, short *buf)
{
	unsigned int sum = 0, i;
	int mean;
	float acc;

	for (i = 0; i < n; i++)
		sum += buf[i];
	mean = (int)(sum / n);
	acc = 0.0f;
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
		flt_out[i] = (float)lin_in[i] * (1.0f / 32000.0f);
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
	mean = (int)(sum / 1000);
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
				acc += (float)(sig[i + j] * pattern[j]);
			corr = (float)(acc * 0.0001);
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
 * The datapump wrapper's per-block conversion: `*countp` samples in each
 * direction at the +/-32000 full scale, then a status word and a constant
 * verdict.  It is the whole function -- there is no filtering here.
 *
 * The count is loaded with `movzwl`, so the pointee is an unsigned short and
 * not a short (finding F613's forced case); the sixth argument's slot is
 * never read here.  Its TYPE comes from `voice_online`, a sibling with this
 * signature slot for slot which does write it -- see beepgen.h, finding F8786
 * and deviation D986.
 */
int
FDSP_DP_Run(int *status, short *rx_lin, float *rx_flt, float *tx_flt,
	    short *tx_lin, unsigned short *hostcount, unsigned short *countp)
{
	int n = *countp;
	int i;

	(void)hostcount;
	for (i = 0; i < n; i++)
		rx_flt[i] = (float)rx_lin[i] * (1.0f / 32000.0f);
	for (i = 0; i < n; i++)
		tx_lin[i] = (short)(tx_flt[i] * 32000.0f);
	*status = 2;
	return 1;
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

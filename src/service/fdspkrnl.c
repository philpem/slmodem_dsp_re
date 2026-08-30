/*
 * fdspkrnl.c -- the full-duplex speakerphone kernel (blob span `Fdspkrnl.c`):
 * a two-direction block LMS echo canceller with an energy gate, plus the
 * TONE_* helpers of the same TU.
 *
 * Emission order follows the object: EchoCanceler 0xae8c0,
 * zFLTUTL_FloatMemSet 0xaea70, bValidateEnergyValue 0xaebe0,
 * FDSP_Kernel_Loop 0xaed10, TONE_generate 0xaf8f0, TONE_filter 0xafba0,
 * TONE_kill 0xafc60.  FDSP_Kernel_InitObj (0xaea90) and MTK_phasor
 * (0xb0690) sit between/after and stay the blob's for now.
 *
 * All floating point here is x87-shaped: every accumulation runs at
 * register precision and narrows only at the stores, on both compilers this
 * tree builds with.
 */

#include <math.h>

#include "dsplib/fdspkrnl.h"
#include "dsplib/beepgen.h"
#include "dsplib/debug.h"
#include "dsplib/sysdep.h"

/*
 * Set by the beep path while a locally generated beep is in progress (its
 * writer, at 0xaea27, is not reconstructed yet); read here to suppress the
 * energy gate.  LOCAL in the blob; external here so the differential tests
 * can drive it, the same trade the functions themselves make.
 */
int bInternalBeepInProgress;

/* (x) > 0 ? (x) : -(x) -- the object's conditional-negate shape. */
#define FDSP_FABS(x) ((x) > 0.0f ? (x) : -(x))

/*
 * One direction over one 160-sample block.
 *
 * The reference window slides DOWN the history one sample per output
 * sample: sample i is predicted from hist[offset+159-i .. offset+159-i+
 * ntaps-1], which with FDSP_Kernel_Loop's reversed-block delay line is the
 * usual convolution direction.
 *
 * The near-side activity test is against HALF the peak |reference| in the
 * current window -- a sample quieter than that is counted, and the taps
 * only adapt on such samples (LMS with a crude double-talk detector).
 *
 * LOCAL in the blob, so regparm(2) there; external linkage and the
 * ordinary convention here (see t_dialstring.c for the precedent).
 */
void
EchoCanceler(float *hist, int offset, float *coef, unsigned int ntaps,
	     float *in, float *out, float *out2, int *verdict,
	     float mu, int update)
{
	unsigned int i;
	int quiet = 0;
	int pos = offset + 159;

	for (i = 0; i <= 159; i++, pos--) {
		float acc = 0.0f;
		float x, e, peak;
		unsigned int k;

		if (mu != 0.0f) {
			unsigned short j;

			for (j = 0; j < ntaps; j++)
				acc += coef[j] * hist[pos + j];
		}
		x = in[i];
		e = x - acc;
		out2[i] = e;
		out[i] = e;

		peak = FDSP_FABS(hist[pos]);
		for (k = 1; k < ntaps; k++) {
			if (FDSP_FABS(hist[pos + k]) > peak)
				peak = FDSP_FABS(hist[pos + k]);
		}
		peak = peak * 0.5f;
		if (fabs(x) < peak)
			quiet++;

		if (update) {
			if (mu != 0.0f && fabs(x) < peak) {
				float g = e * mu;
				unsigned int m;

				for (m = 0; m < ntaps; m++)
					coef[m] += g * hist[pos + m];
			}
		}
	}
	*verdict = quiet > 80;
}

/* buf[0..n-1] = v. */
void
zFLTUTL_FloatMemSet(float v, float *buf, unsigned int n)
{
	unsigned int i;

	for (i = 0; i < n; i++)
		buf[i] = v;
}

/*
 * The energy gate in front of each EchoCanceler run.
 *
 * The block's mean-removed power (fComputeRMSValueFloatBuf, scaled by
 * 32000) goes into hist[] as an int, and the gate judges the AVERAGE of the
 * ring.  Loud input starts (or continues) a countdown of k->saturation
 * blocks; when it expires the whole kernel is re-initialised -- the
 * object's own debug line calls that the "Delayed FDSP_Kernel_InitObj
 * invocation due to saturation".  Only a quiet average with no countdown
 * pending returns 1, which is what licenses tap adaptation.
 *
 * LOCAL in the blob (regparm(2) there); ordinary convention here.
 */
int
bValidateEnergyValue(float *buf, unsigned int n, int *hist,
		     unsigned int *idxp, unsigned int histlen,
		     struct fdsp_kernel *k)
{
	unsigned int sum = 0, i;
	int avg;

	if (bInternalBeepInProgress)
		return 0;

	hist[*idxp] = (int)(fComputeRMSValueFloatBuf(n, buf) * 32000.0f);
	*idxp = (*idxp + 1) % histlen;
	for (i = 0; i < histlen; i++)
		sum += hist[i];
	avg = sum / histlen;

	if (avg > 2200) {
		if (k->saturation == 0) {
			if (DSPLIB_DEBUG_ON())
				dsplibs_debug_printf(
				    "Identify High energy\n");
			k->saturation = 0x500 / n * 2;
		} else {
			k->saturation--;
		}
	} else {
		if (k->saturation == 0)
			return 1;
		k->saturation--;
	}
	if (k->saturation == 0) {
		FDSP_Kernel_InitObj(k);
		if (DSPLIB_DEBUG_ON())
			dsplibs_debug_printf(
			    "Delayed FDSP_Kernel_InitObj invocation due to saturation. \n");
	}
	return 0;
}

/*
 * One block through both directions.
 *
 * Each channel's delay line holds its REFERENCE signal -- the other
 * direction's input -- in blocks of 160, each block reversed, newest at
 * dly[0].  So the shift moves everything one block up, the cancel runs
 * against the previous blocks, and the fresh reference lands reversed at
 * the front for next time.
 *
 * The error of each direction also lands in the OTHER channel's `cross`
 * block; nothing reconstructed reads it back yet.
 */
int
FDSP_Kernel_Loop(struct fdsp_kernel *k, float *in_a, float *out_b,
		 float *in_b, float *out_a)
{
	struct fdsp_channel *a, *b;
	float rev_a[FDSP_BLOCK], rev_b[FDSP_BLOCK];
	unsigned int i;
	int valid;

	a = k->chan_a;
	for (i = FDSP_DLY - 1; i >= FDSP_BLOCK; i--)
		a->dly[i] = a->dly[i - FDSP_BLOCK];
	b = k->chan_b;
	for (i = FDSP_DLY - 1; i >= FDSP_BLOCK; i--)
		b->dly[i] = b->dly[i - FDSP_BLOCK];

	a = k->chan_a;
	valid = bValidateEnergyValue(in_b, FDSP_BLOCK, a->energy,
				     &a->energy_idx, 4, k);
	a = k->chan_a;
	b = k->chan_b;
	EchoCanceler(a->dly, a->offset, a->coef, k->ntaps_a, in_a, out_a,
		     b->cross, &a->verdict, a->mu, valid);

	b = k->chan_b;
	valid = bValidateEnergyValue(in_a, FDSP_BLOCK, b->energy,
				     &b->energy_idx, 4, k);
	b = k->chan_b;
	a = k->chan_a;
	EchoCanceler(b->dly, b->offset, b->coef, k->ntaps_b, in_b, out_b,
		     a->cross, &b->verdict, b->mu, valid);

	for (i = 0; i < FDSP_BLOCK; i++)
		rev_a[i] = in_b[FDSP_BLOCK - 1 - i];
	sysdep_memcpy(k->chan_a->dly, rev_a, sizeof(rev_a));
	for (i = 0; i < FDSP_BLOCK; i++)
		rev_b[i] = in_a[FDSP_BLOCK - 1 - i];
	sysdep_memcpy(k->chan_b->dly, rev_b, sizeof(rev_b));
	return 1;
}

/*
 * n samples of tone: MTK_phasor advances the oscillator, `amp` scales it,
 * and a millisecond clock (0.125 ms per sample: 8 kHz) runs against
 * `duration`.  When a positive duration expires the clock resets and the
 * phase jumps by pi -- the beep cadence inverts the tone rather than
 * gating it -- wrapping against the object's own 6.28318530718, whose
 * missing digits are the author's and are kept.
 */
void
TONE_generate(struct fdsp_tone *t, float *buf, short n)
{
	struct mtk_phasor ph;
	float tm, p;
	short i;

	ph.phase = t->phase;
	ph.step = t->step;
	i = n;
	while (i--) {
		MTK_phasor(&ph);
		*buf++ = ph.out_08 * t->amp;
	}
	tm = (float)n * 0.125f + t->elapsed;
	if (t->duration > tm || 0.0f >= t->duration) {
		t->elapsed = tm;
		t->phase = ph.phase;
		return;
	}
	t->elapsed = 0.0f;
	p = ph.phase + 3.141592653589793;
	if (p > 6.28318530718)
		p = p - 6.28318530718;
	ph.phase = p;
	t->phase = ph.phase;
}

/*
 * In-place FIR over the object's ring delay line.  The coefficient pointer
 * runs FORWARD while the delay index runs backward from the newest sample,
 * wrapping once -- the standard circular MAC, with the index update
 * branchless in the object and a plain conditional here.
 */
void
TONE_filter(struct fdsp_tone *t, float *buf, short n)
{
	float *dly = t->fir_dly;
	short len = t->fir_len;
	short idx = t->fir_idx;
	short i;

	i = n;
	while (i--) {
		const float *c = t->fir_coef;
		float acc = 0.0f;
		int j;

		idx = (short)(idx + 1) < len ? (short)(idx + 1) : 0;
		dly[idx] = *buf;
		for (j = idx; j >= 0; j--)
			acc += *c++ * dly[j];
		for (j = len - 1; j > idx; j--)
			acc += *c++ * dly[j];
		*buf++ = acc;
	}
	t->fir_idx = idx;
}

/*
 * In-place two-pole section: with c = iir_coef,
 *
 *   w   = c[1]*z2 + x + c[2]*z1
 *   y   = c[0]*z2 + w + z1
 *   z1' = z2, z2' = w
 *
 * The sum ORDER above is the object's fadd sequence.  It is carried by the
 * disassembly rather than by the tests: at x87 extended precision a
 * reassociation shifts the result by ~2^-64 relative, which the narrowing
 * store to float erases -- see the fdspkrnl mutation file's note.
 */
void
TONE_kill(struct fdsp_tone *t, float *buf, short n)
{
	const float *c = t->iir_coef;
	short i;

	i = n;
	while (i--) {
		float w = c[1] * t->iir_z2 + *buf + c[2] * t->iir_z1;
		float old2;

		*buf = c[0] * t->iir_z2 + w + t->iir_z1;
		old2 = t->iir_z2;
		t->iir_z2 = w;
		t->iir_z1 = old2;
		buf++;
	}
}

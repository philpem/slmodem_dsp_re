/*
 * fdspkrnl.c -- the full-duplex speakerphone kernel (blob span `Fdspkrnl.c`):
 * a two-direction block LMS echo canceller with an energy gate, plus the
 * TONE_* helpers of the same TU.
 *
 * FDSP_DP_Create and FDSP_DP_Delete are here and not in beepgen.c although
 * their span is `Beepgen.c +3`: the span is the blob's layout and the NAME
 * belongs with the kernel, which is the same call `include/dsplib/beepgen.h`
 * records for FDSP_DP_Run going the other way.
 *
 * Emission order follows the object: FDSP_DP_Delete 0xae520,
 * FDSP_DP_Create 0xae5c0, EchoCanceler 0xae8c0,
 * FDSP_Kernel_SetInternalBeepInProgress 0xaea30, zFLTUTL_FloatMemSet
 * 0xaea70, FDSP_Kernel_InitObj 0xaea90, bValidateEnergyValue 0xaebe0,
 * FDSP_Kernel_Loop 0xaed10, TONE_create 0xaf690, TONE_delete 0xaf8a0,
 * TONE_generate 0xaf8f0, TONE_detect 0xaf9f0, TONE_filter 0xafba0,
 * TONE_kill 0xafc60.
 *
 * The span's other reconstructed neighbours are NOT here: the FIFO8 ring
 * (0xaef30..0xaf150) is in fifo8.c and the silence detector with `_status`
 * (0xb02e0..0xb0415) is in silence.c, following the split the tree had
 * already made for voice_* .  MTK_phasor (0xb0690) is in mtk.c.
 *
 * TONE_CFG (`.data` 0x83c0) and the `ToneLPF` it points at (`.data` 0x8400)
 * are here because TONE_create is: the global config and the local array
 * sit together in the object between fifo8.c's FIFO_CFG at 0x83a0 and
 * silence.c's table at 0x84d4.
 *
 * All floating point here is x87-shaped: every accumulation runs at
 * register precision and narrows only at the stores, on both compilers this
 * tree builds with.
 */

#include <math.h>
#include <string.h>

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

/*
 * The one kernel FDSP_DP_Create hands out, and a counter that is written
 * and never read.
 *
 * Both are `b` in the blob and both are external here, which is the trade
 * `bInternalBeepInProgress` above already makes: a `static` cannot be
 * compared against the blob's, and the blob's copy is globalized and
 * renamed by `symmap.py` so the two sides keep their own.
 *
 * `uCorrelationReportsNo` has exactly ONE relocation against it in the
 * whole 1.2 MB -- the store at 0xae61d, below -- so nothing in the object
 * ever reads it back.  Its name is the author's and its purpose is not
 * established by anything here.
 */
struct fdsp_kernel *pGlobalFDSPObj;
unsigned int uCorrelationReportsNo;

/*
 * Tear the kernel down: the buffer block, then each channel's taps and the
 * channel itself, then the kernel, then the global.
 *
 * THE CHANNEL POINTERS ARE DEREFERENCED BEFORE THEY ARE TESTED.  `mov
 * 0x14(%ebx),%eax; mov 0x1680(%eax),%edx` at 0xae533 reads the taps out of
 * `chan_a` and only then does 0xae540 ask whether `chan_a` was NULL, and
 * `chan_b` is the same three instructions later.  So the NULL tests protect
 * `sysdep_free` and nothing else, and a kernel with a missing channel
 * faults here rather than being freed.  It is the object's shape and is
 * written as the object has it; the only caller that can produce such a
 * kernel is FDSP_DP_Create's own out-of-memory path, which the harness
 * allocator cannot drive.
 */
void
FDSP_DP_Delete(struct fdsp_kernel *k)
{
	if (k == 0)
		return;
	if (k->buffers != 0)
		sysdep_free(k->buffers);
	if (k->chan_a->coef != 0)
		sysdep_free(k->chan_a->coef);
	if (k->chan_a != 0)
		sysdep_free(k->chan_a);
	if (k->chan_b->coef != 0)
		sysdep_free(k->chan_b->coef);
	if (k->chan_b != 0)
		sysdep_free(k->chan_b);
	sysdep_free(k);
	pGlobalFDSPObj = 0;
}

/*
 * Create or re-initialise the kernel.
 *
 * A NULL `k` allocates the whole tree -- kernel, buffer block, both
 * channels, both tap arrays -- and a non-NULL one is simply re-initialised,
 * which is what the object's own "Reinitialization requested" line reports.
 * Either way the object ends up in FDSP_Kernel_InitObj's state and in
 * `pGlobalFDSPObj`.
 *
 * THE TWO ARGUMENTS ARE NAMED BY THE OBJECT'S OWN FORMAT STRING, which is
 * "ver 120 sRxSamplesDelay %d ,sTxSamplesDelay %d \n" at `.rodata.str1.4`
 * 0x12f30 -- so the rx delay is chan_a's window offset and the tx delay is
 * chan_b's, and that is also what fixes which channel is which direction.
 * Both are `short` (`movswl` at 0xae5de and 0xae5e3).
 *
 * `status` is 2 unless the RX delay is NEGATIVE, in which case it is 0:
 * `sar $0x1f; not; and $0x2` is a branchless `(rx >= 0) ? 2 : 0`, and it
 * lands on the field InitObj has just set to 2.
 *
 * THE ALLOCATION CHAIN'S FAILURE ARMS CANNOT BE DRIVEN HERE.  Six
 * allocations each guard the next, and the harness allocator does not fail
 * on request, so `t_fdspdp` exercises the success path and the
 * re-initialisation path and records the rest.  Two of them are worse than
 * a leak and are written as the object has them: the two `offset` stores at
 * 0xae6c2 happen BEFORE the chain's result is tested, so a failed channel
 * allocation is dereferenced there; and the cleanup that follows is
 * FDSP_DP_Delete, whose own unguarded dereference is described above.
 */
struct fdsp_kernel *
FDSP_DP_Create(struct fdsp_kernel *k, short sRxSamplesDelay,
	       short sTxSamplesDelay)
{
	if (DSPLIB_DEBUG_ON())
		dsplibs_debug_printf("ver 120 sRxSamplesDelay %d ,"
				     "sTxSamplesDelay %d \n",
				     sRxSamplesDelay, sTxSamplesDelay);
	if (k == 0) {
		int ok;

		k = (struct fdsp_kernel *)sysdep_malloc(sizeof(*k));
		ok = k != 0;
		if (ok) {
			k->chan_b = 0;
			k->chan_a = 0;
			k->buffers = 0;
		}
		if (ok) {
			k->buffers = (struct fdsp_buffers *)
				     sysdep_malloc(sizeof(*k->buffers));
			ok = k->buffers != 0;
		}
		if (ok) {
			k->chan_b = (struct fdsp_channel *)
				    sysdep_malloc(sizeof(*k->chan_b));
			ok = k->chan_b != 0;
		}
		if (ok) {
			k->chan_a = (struct fdsp_channel *)
				    sysdep_malloc(sizeof(*k->chan_a));
			ok = k->chan_a != 0;
		}
		if (ok) {
			k->chan_b->coef = (float *)
					  sysdep_malloc(240 * sizeof(float));
			ok = k->chan_b->coef != 0;
		}
		if (ok) {
			k->chan_a->coef = (float *)
					  sysdep_malloc(240 * sizeof(float));
			ok = k->chan_a->coef != 0;
		}
		k->chan_a->offset = sRxSamplesDelay;
		k->chan_b->offset = sTxSamplesDelay;
		if (!ok) {
			FDSP_DP_Delete(k);
			k = 0;
		}
	} else if (DSPLIB_DEBUG_ON()) {
		dsplibs_debug_printf("Reinitialization requested: \n");
	}

	if (k != 0) {
		FDSP_Kernel_InitObj(k);
		k->status = (sRxSamplesDelay >= 0) ? 2 : 0;
	}
	pGlobalFDSPObj = k;
	uCorrelationReportsNo = 0;
	return k;
}

/*
 * chan_a's LMS step, as FDSP_Kernel_InitObj stores it: `mov $0x3d03126e`.
 *
 * IT IS NOT `0.032f`.  fl(0.032) is 0x3d03126f -- 0.032 sits at .592 of the
 * way between two floats, so correct rounding goes UP and the object's word
 * is one ULP BELOW it.  Whatever produced 0x3d03126e truncated rather than
 * rounded, so the tidy literal is not what the author's compiler emitted and
 * writing it here costs the differential test (it fails at 1 ULP, which is
 * exactly the size of the disagreement).  What is written is the object's
 * float spelled exactly, so any correctly-rounding compiler reproduces the
 * word.  Finding F8750.
 */
#define FDSP_CHAN_A_MU	0.031999997794628143310546875f

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

/*
 * Raise or drop the beep flag.  The store happens whatever the debug level
 * is -- the gate is only around the message, and the object computes the
 * "ON"/"OFF" pointer inside it.
 */
void
FDSP_Kernel_SetInternalBeepInProgress(int on)
{
	bInternalBeepInProgress = on;
	if (DSPLIB_DEBUG_ON())
		dsplibs_debug_printf("Internal Beep %s \n", on ? "ON" : "OFF");
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
 * Everything back to its starting state, and the fixed defaults with it.
 *
 * Nothing is allocated here: the two channel pointers, their `coef` arrays
 * and `buffers` must all be set before the call, and all four are
 * dereferenced.  The tap count is a CONSTANT of the object -- 80 on chan_a
 * and 40 on chan_b, cleared 240 at a time regardless -- so the 240 is the
 * allocation size the creator must have used, not k->ntaps_*.
 *
 * chan_a adapts (see FDSP_CHAN_A_MU) and chan_b does not; EchoCanceler skips
 * both the convolution and the tap update when mu is zero, so chan_b's
 * direction passes through until something raises its step.
 */
void
FDSP_Kernel_InitObj(struct fdsp_kernel *k)
{
	struct fdsp_channel *a, *b;
	struct fdsp_buffers *p;
	unsigned int i;

	p = k->buffers;
	for (i = 0; i < 2000; i++) {
		p->short_0000[i] = 0;
		p->short_0fa0[i] = 0;
	}

	b = k->chan_b;
	b->energy_idx = 0;
	for (i = 0; i < 4; i++)
		b->energy[i] = 0;

	a = k->chan_a;
	a->energy_idx = 0;
	for (i = 0; i < 4; i++)
		a->energy[i] = 0;

	k->saturation = 0;
	p->int_2710 = 0;
	p->int_2714 = 0;
	p->int_2718 = 0;

	k->status = 2;
	k->ntaps_a = 80;
	k->ntaps_b = 40;
	a->mu = FDSP_CHAN_A_MU;
	b->mu = 0.0f;
	a->short_1690 = 0;
	b->short_1690 = 0;

	for (i = 0; i < FDSP_DLY; i++)
		b->dly[i] = 0.0f;
	for (i = 0; i < 240; i++)
		b->coef[i] = 0.0f;
	for (i = 0; i < FDSP_DLY; i++)
		a->dly[i] = 0.0f;
	for (i = 0; i < 240; i++)
		a->coef[i] = 0.0f;
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
	avg = (int)(sum / histlen);

	if (avg > 2200) {
		if (k->saturation == 0) {
			if (DSPLIB_DEBUG_ON())
				dsplibs_debug_printf(
				    "Identify High energy\n");
			k->saturation = (int)(0x500 / n * 2);
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
 * The FIR prototype TONE_CFG points at: 53 symmetric taps, `.data` 0x8400,
 * 212 bytes, LOCAL in the object and so `static` here.
 *
 * IT IS NOT THE `ToneLPF` `src/dsp/fpm_tone_cfg.c` ALREADY HAS.  The blob
 * carries TWO symbols of that name -- `r` at 0xd040, 53 SHORTS, the fixed
 * point pump's; and `d` at 0x8400, 53 FLOATS, this one -- and they are the
 * same filter at different quantisations rather than a copy: the float
 * peak is 0.0418356508 and the short peak is 1370, and 0.0418356508 * 32768
 * is 1370.9, so neither was derived from the other by scaling.  Both being
 * local is also why neither gets a `ref_` alias: `symmap.py` refuses to
 * globalize a name two translation units use, so this array is compared
 * through TONE_CFG's pointer to it instead.
 *
 * Byte-exact, not derived.
 */
static const float ToneLPF[53] = {
	-0.00173766701f, -0.00138069503f, -0.000847295218f,
	-0.000120461002f, 0.000813306484f, 0.00196331297f,
	0.00333407498f, 0.0049248389f, 0.00672924099f,
	0.00873511378f, 0.0109244697f, 0.0132736498f,
	0.0157536492f, 0.0183306299f, 0.0209665503f,
	0.0236200206f, 0.0262471903f, 0.0288028009f,
	0.0312412605f, 0.0335178189f, 0.0355896801f,
	0.0374171399f, 0.0389645882f, 0.0402014889f,
	0.0411031917f, 0.0416515991f, 0.0418356508f,
	0.0416515991f, 0.0411031917f, 0.0402014889f,
	0.0389645882f, 0.0374171399f, 0.0355896801f,
	0.0335178189f, 0.0312412605f, 0.0288028009f,
	0.0262471903f, 0.0236200206f, 0.0209665503f,
	0.0183306299f, 0.0157536492f, 0.0132736498f,
	0.0109244697f, 0.00873511378f, 0.00672924099f,
	0.0049248389f, 0.00333407498f, 0.00196331297f,
	0.000813306484f, -0.000120461002f, -0.000847295218f,
	-0.00138069503f, -0.00173766701f
};

/*
 * The default tone: 2100 Hz for 450 ms, which is V.25's answer tone.
 *
 * `D` in the object at `.data` 0x83c0 and not `R`, so it is not `const`;
 * that is the section deciding the qualifier rather than a preference, and
 * `const` would move it to `.rodata`.
 */
struct fdsp_tone_cfg TONE_CFG = {
	2100.0f,		/* +0x00 freq                            */
	0.850000024f,		/* +0x04 amp                             */
	450.0f,			/* +0x08 duration, ms                    */
	0.75f,			/* +0x0c                                 */
	0.00999999978f,		/* +0x10                                 */
	0.00749999983f,		/* +0x14                                 */
	0.9375f,		/* +0x18 pole radius                     */
	ToneLPF,		/* +0x1c                                 */
	53,			/* +0x20 fir_len, and ToneLPF's length    */
	/* +0x22 was the `pad_22[2]` initializer; the two bytes are
	 * compiler-inserted alignment now (finding F10151) and no longer
	 * have a positional slot of their own. */
	0,			/* +0x24 */
	0x3f000000,		/* +0x28 0.5f as a word; nothing
				 *       reconstructed reads it, so the
				 *       TYPE is not established         */
	0x28			/* +0x2c                                 */
};

/*
 * Build a tone object.
 *
 * Four things happen, and the third is the one worth reading twice.
 *
 * 1. A NULL `t` is allocated, 0x1c8 bytes, and a NULL `cfg` becomes
 *    TONE_CFG.  The allocation is NOT checked -- FIFO8_create's shape, not
 *    silence_create's -- and the copy below dereferences it at once.
 * 2. The configuration's 48 bytes are copied over the head of the object.
 * 3. The four heap blocks are allocated only when THIS CALL allocated the
 *    object AND `fir_len` is positive (`test %edx,%eax` at 0xaf6d9 over
 *    two `set` results).  A caller passing its own storage gets the filter
 *    built into whatever pointers that storage already held.
 * 4. The oscillator, the FIR and the two detector sections are seeded.
 *
 * THE TWO PHASORS ARE SEPARATE OBJECTS and only one of them is zeroed at
 * entry (`rep stos` with `$0x4` in `%ecx`, four words).  The 60 Hz one is;
 * the tone's has its `phase` and `step` written before either use.
 */
struct fdsp_tone *
TONE_create(struct fdsp_tone *t, const struct fdsp_tone_cfg *cfg)
{
	struct mtk_phasor hum = { 0.0f, 0.0f, 0.0f, 0.0f };
	struct mtk_phasor osc;
	float *biquad;
	short allocated = 0;
	short i;

	if (t == 0) {
		t = (struct fdsp_tone *)sysdep_malloc(sizeof(*t));
		allocated = 1;
	}
	if (cfg == 0)
		cfg = &TONE_CFG;
	memcpy(t, cfg, sizeof(*cfg));

	if (allocated && t->fir_len > 0) {
		t->fir_coef = (float *)sysdep_malloc(t->fir_len * 4);
		t->fir_dly = (float *)sysdep_malloc(t->fir_len * 4);
		t->ptr_01b4 = (float *)sysdep_malloc(20);
		t->ptr_01b8 = (float *)sysdep_malloc(8);
	}

	/*
	 * 0.0007853981633975 is 2*pi/8000 as the author typed it -- a
	 * DOUBLE, and three digits short of the nearest one, which is the
	 * same habit as MTK_phasor's 6.28318530718 (finding F8780).
	 */
	osc.phase = (float)(t->freq * 0.0007853981633975);
	osc.step = 0.0f;
	MTK_phasor(&osc);
	t->phase = 0.0f;
	t->step = osc.phase;
	t->elapsed = 0.0f;

	/*
	 * A resonator at the tone's frequency with the configured pole
	 * radius: the denominator is 1 - 2*r*cos(w) z^-1 + r^2 z^-2 and
	 * these are its coefficients with the signs the filters here use.
	 */
	t->det_coef[0] = -2.0f * osc.cosine;
	t->det_coef[1] = -t->det_coef[0] * t->pole_radius;
	t->det_coef[2] = t->pole_radius * -t->pole_radius;
	t->det_z1 = 0.0f;
	t->det_z2 = 0.0f;
	t->float_005c = 0.0f;
	t->float_0060 = 0.0f;
	t->fir_idx = 0;

	/*
	 * The FIR is the prototype modulated by the tone and doubled, and
	 * the delay line is cleared in the same pass.  `osc.step` is the
	 * reduced angle the call above left in `osc.phase`, so every tap
	 * advances by one sample's worth of phase.
	 */
	osc.step = osc.phase;
	for (i = 0; i < t->fir_len; i++) {
		MTK_phasor(&osc);
		t->fir_dly[i] = 0.0f;
		t->fir_coef[i] = osc.cosine * t->fir_proto[i] * 2.0f;
	}

	t->short_0064 = 0;
	t->int_0068 = 0;
	t->short_01b0 = 0;
	t->int_006c = 0;
	for (i = 0; i <= 79; i++)
		t->int_0070[i] = 0;
	t->iir_z1 = 0.0f;
	t->iir_coef = t->det_coef;
	t->iir_z2 = 0.0f;

	/*
	 * The output section is a 60 Hz notch with poles at radius 0.96:
	 * 0.04712389f is 2*pi*60/8000, -0.9215999841690063f is -0.96*0.96
	 * and the 1.92 is 2*0.96.  It is written into the 20-byte block as
	 * five floats and its state into the 8-byte one.
	 */
	hum.phase = 0.04712389f;
	MTK_phasor(&hum);
	biquad = t->ptr_01b4;
	biquad[1] = 1.0f;
	biquad[0] = -0.9215999841690063f;
	biquad[2] = (float)(1.92 * hum.cosine);
	biquad[4] = 1.0f;
	biquad[3] = -2.0f * hum.cosine;
	t->ptr_01b8[0] = 0.0f;
	t->ptr_01b8[1] = 0.0f;
	return t;
}

/*
 * Free the object and, when it has a filter, the four blocks that hang off
 * it.  `fir_len > 0` is the whole test: the object frees +0x1b8 and +0x1b4
 * -- two pointers nothing reconstructed reads -- and then the FIR's delay
 * line and coefficients, in that order.
 */
void
TONE_delete(struct fdsp_tone *t)
{
	if (t->fir_len > 0) {
		sysdep_free(t->ptr_01b8);
		sysdep_free(t->ptr_01b4);
		sysdep_free(t->fir_dly);
		sysdep_free(t->fir_coef);
	}
	sysdep_free(t);
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
		*buf++ = ph.sine * t->amp;
	}
	tm = (float)n * 0.125f + t->elapsed;
	if (t->duration > tm || 0.0f >= t->duration) {
		t->elapsed = tm;
		t->phase = ph.phase;
		return;
	}
	t->elapsed = 0.0f;
	p = (float)(ph.phase + 3.141592653589793);
	if (p > 6.28318530718)
		p = (float)(p - 6.28318530718);
	ph.phase = p;
	t->phase = ph.phase;
}

/*
 * The smoothing pole of TONE_detect's two power estimates.  Both constants
 * are DOUBLES in the object, and the second is not the double nearest 0.05
 * -- it is 0.050000000000000044, exactly fl(1.0 - 0.95).  That is a
 * compile-time fold of the complement, so the author wrote one constant and
 * derived the other; the spelling here reproduces the fold.
 */
#define TONE_DETECT_POLE	0.95

/*
 * The tone verdict.
 *
 * Per sample: the FIR of TONE_filter (same ring, same wrap, same two-part
 * MAC -- and the same state fields, so a TONE_filter and a TONE_detect on
 * one object would fight over `fir_idx`), then a two-pole section whose
 * coefficients live inline at +0x48 and whose arithmetic is TONE_kill's.
 *
 * Two one-pole power estimates come out of it: `e_tot` follows the FIR
 * output's square, `e_res` follows the DIFFERENCE of the two squares.  Both
 * are `float` locals, so each pass narrows them -- the object spills and
 * reloads a 4-byte slot at exactly those two points, which is not something
 * a wider local would do.
 *
 * The verdict is then a floor test and a ratio test; see the header.
 */
int
TONE_detect(struct fdsp_tone *t, float *buf, short n)
{
	float *dly = t->fir_dly;
	const float *coef = t->fir_coef;
	short len = t->fir_len;
	short idx = t->fir_idx;
	float e_res = t->float_005c;
	float e_tot = t->float_0060;
	short i;

	i = n;
	while (i--) {
		const float *c = coef;
		float acc = 0.0f;
		float w, y;
		int j;

		idx = (short)((short)(idx + 1) < len ? (short)(idx + 1) : 0);
		dly[idx] = *buf++;
		for (j = idx; j >= 0; j--)
			acc += *c++ * dly[j];
		for (j = len - 1; j > idx; j--)
			acc += *c++ * dly[j];

		w = t->det_coef[1] * t->det_z2 + acc +
		    t->det_coef[2] * t->det_z1;
		y = t->det_coef[0] * t->det_z2 + w + t->det_z1;
		t->det_z1 = t->det_z2;
		t->det_z2 = w;

		e_res = (float)(e_res * TONE_DETECT_POLE +
			(acc * acc - y * y) * (1 - TONE_DETECT_POLE));
		e_tot = (float)(e_tot * TONE_DETECT_POLE +
			acc * acc * (1 - TONE_DETECT_POLE));
	}
	t->fir_idx = idx;

	if (0.0f > e_res)
		e_res = 0.0f;
	t->float_005c = e_res;
	t->float_0060 = e_tot;

	if (e_tot < t->float_0014)
		return 2;
	return e_tot * t->float_000c >= e_res;
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

		idx = (short)((short)(idx + 1) < len ? (short)(idx + 1) : 0);
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

/*
 * The reserved regions are byte counts from a 32-bit build, so these are
 * compiled only under that ABI -- the same guard fpm_tone.c carries, and
 * the same reason.
 *
 * `struct fdsp_tone_cfg` is asserted against `struct fdsp_tone` field by
 * field as well as by size, because TONE_create copies one over the other
 * with a single 48-byte move: if the two layouts ever part, nothing in the
 * suite would fail on the fields the copy silently misplaced.
 */
#if defined(__SIZEOF_POINTER__) && __SIZEOF_POINTER__ == 4

#define TONE_ASSERT_OFF(field, off) \
	typedef char fdsp_tone_off_##field[ \
		((int)__builtin_offsetof(struct fdsp_tone, field) == (off)) \
			? 1 : -1]

#define TONE_ASSERT_CFG(field) \
	typedef char fdsp_tone_cfg_off_##field[ \
		((int)__builtin_offsetof(struct fdsp_tone_cfg, field) \
		 == (int)__builtin_offsetof(struct fdsp_tone, field)) ? 1 : -1]

/*
 * Same idea as TONE_ASSERT_OFF, but for a `struct fdsp_tone_cfg` field with
 * no same-named counterpart in `struct fdsp_tone` to cross-check against --
 * `int_0024` lands inside the span `fdsp_tone::pad_22[0xe]` still leaves
 * unmodelled, so it can only be asserted against its own struct's offset.
 * Finding F10151.
 */
#define TONE_ASSERT_OFF_CFG(field, off) \
	typedef char fdsp_tone_cfg_offx_##field[ \
		((int)__builtin_offsetof(struct fdsp_tone_cfg, field) \
			== (off)) ? 1 : -1]

TONE_ASSERT_OFF(freq, 0x000);
TONE_ASSERT_OFF(pole_radius, 0x018);
TONE_ASSERT_OFF(fir_proto, 0x01c);
TONE_ASSERT_OFF(fir_len, 0x020);
TONE_ASSERT_OFF(det_coef, 0x048);
TONE_ASSERT_OFF(short_0064, 0x064);
TONE_ASSERT_OFF(int_0068, 0x068);
TONE_ASSERT_OFF(int_006c, 0x06c);
TONE_ASSERT_OFF(int_0070, 0x070);
TONE_ASSERT_OFF(short_01b0, 0x1b0);
TONE_ASSERT_OFF(ptr_01b4, 0x1b4);
TONE_ASSERT_OFF(iir_coef, 0x1bc);
TONE_ASSERT_OFF(iir_z2, 0x1c4);

TONE_ASSERT_CFG(freq);
TONE_ASSERT_CFG(amp);
TONE_ASSERT_CFG(duration);
TONE_ASSERT_CFG(float_000c);
TONE_ASSERT_CFG(float_0014);
TONE_ASSERT_CFG(pole_radius);
TONE_ASSERT_CFG(fir_proto);
TONE_ASSERT_CFG(fir_len);
TONE_ASSERT_OFF_CFG(int_0024, 0x024);

/* the object's own sizes: 0x1c8 from the malloc, 0x30 from `rep movsl` */
typedef char fdsp_tone_size[(sizeof(struct fdsp_tone) == 0x1c8) ? 1 : -1];
typedef char fdsp_tone_cfg_size[(sizeof(struct fdsp_tone_cfg) == 0x30)
				? 1 : -1];

#endif

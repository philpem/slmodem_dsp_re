/* TONE.c -- floating-point tone generation, detection and filtering. */

#include <string.h>

#include "dsplib/fdspkrnl.h"
#include "dsplib/sysdep.h"

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
		t = sysdep_malloc(sizeof(*t));
		allocated = 1;
	}
	if (cfg == 0)
		cfg = &TONE_CFG;
	memcpy(t, cfg, sizeof(*cfg));

	if (allocated && t->fir_len > 0) {
		t->fir_coef = sysdep_malloc(t->fir_len * 4);
		t->fir_dly = sysdep_malloc(t->fir_len * 4);
		t->ptr_01b4 = sysdep_malloc(20);
		t->ptr_01b8 = sysdep_malloc(8);
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

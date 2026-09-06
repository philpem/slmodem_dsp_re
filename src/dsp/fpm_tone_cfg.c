/*
 * fpm_tone_cfg.c -- Fixed Point Modem: tone configuration and filter prototype.
 *
 * GENERATED-ish: extracted from dsplibs.o .rodata (FPM_TONE_CFG at 0xd000,
 * ToneLPF at 0xd040) -- do not edit the numbers.
 *
 * FPM_TONE_CFG is the ITU-T V.25 answer tone: 2100 Hz, reversing phase every
 * 450 ms.  It is a mixed struct, not an array: the field at +0x10 is a
 * POINTER to ToneLPF, which is why dumping the config as int16 misleads (the
 * stored value is only a relocation addend).
 *
 * ToneLPF is a 53-tap lowpass prototype.  FPM_TONE_create modulates it by a
 * cosine at the tone frequency to build a matched bandpass -- the standard
 * construction for a tone detector, and the reason the config carries both a
 * frequency and a prototype.
 */

#include "dsplib/fpm_tone.h"

/* 53 taps; the count is mirrored in FPM_TONE_CFG at +0x14. */
const short ToneLPF[53] = {
	-56, -45, -27, -3, 26, 64, 109, 161,
	220, 286, 357, 434, 516, 600, 687, 773,
	860, 943, 1023, 1098, 1166, 1226, 1276, 1317,
	1346, 1364, 1370, 1364, 1346, 1317, 1276, 1226,
	1166, 1098, 1023, 943, 860, 773, 687, 600,
	516, 434, 357, 286, 220, 161, 109, 64,
	26, -3, -27, -45, -56,
};

/*
 * Laid out to match the original's 36 bytes exactly.  Written as a struct
 * rather than a short[] so the embedded pointer is a pointer, which is the
 * whole point.
 */
const struct fpm_tone_cfg FPM_TONE_CFG = {
	.freq = 2100,		/* the ITU-T V.25 answer tone */
	.scale = 27852,
	.rev_period = 450,	/* 450 ms at 8 kHz, the V.25 figure */
	.ratio = 24576,		/* 0.75 in Q15 */
	.f08 = 328,
	.min_level = 1,
	.damp = 30720,		/* notch pole radius, 0.9375 in Q15 */
	.src = ToneLPF,
	.len = 53,
	/*
	 * These two were carried as `pad16[5]` and are not padding -- the
	 * positional form hid that, and converting to designated initialisers
	 * dropped them, which the differential test caught immediately.  That
	 * is the argument for this form in one line.
	 */
	.rev_thresh = 16384,	/* one half, Q15 */
	.rev_lag = 40		/* samples; 2*40 is `rev_hist`'s 80 words */
	/* r16 and extra are zero */
};

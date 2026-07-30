/*
 * fpm_tone.c -- tone generator and detector.
 *
 * Reconstructed from dsplibs.o fpm_tone.c:
 *   FPM_TONE_set_freq   .text 0x0aaf50
 *   FPM_TONE_set_scale  .text 0x0aaf70
 *   FPM_TONE_generate   .text 0x0aad50
 *
 * Not yet reconstructed: FPM_TONE_delete and the detector half
 * (_detect, _find_rev, _filter, _kill, _generate2, _generate_demod).
 *
 * The object is 0x108 bytes and is treated here as opaque storage accessed at
 * known offsets, because most of its fields belong to the detector.  That is
 * deliberate: naming a struct now would mean guessing at fields whose meaning
 * has not been established, and a wrong layout is worse than none.
 */

#include <stddef.h>

#include "dsplib/fpm_tone.h"
#include "dsplib/fpm_phasor.h"

extern void *sysdep_malloc(unsigned size);
extern void sysdep_free(void *ptr);
extern void *sysdep_memcpy(void *dst, const void *src, unsigned n);

/* Little-endian field access into the opaque object. */
static short *
fld(void *state, int off)
{
	return (short *)((char *)state + off);
}

/*
 * Hz to phase increment.  A cycle is FPM_PHASOR_CYCLE (0x8000) units, so the
 * increment is hz * 32768 / fs -- and the original hard-codes fs = 8000 as
 * the constant 0x8312 in Q13 (33554 / 8192 = 4.096 = 32768 / 8000).
 *
 * Reproduced exactly, including the +0x1000 rounding bias.  Recorded as R-9
 * in docs/rate_assumptions.md; do not "fix" it without reading that entry.
 */
void
FPM_TONE_set_freq(void *state, short hz)
{
	*fld(state, FPM_TONE_OFF_INC) = (short)(((int)hz * 0x8312 + 0x1000) >> 13);
}

void
FPM_TONE_set_scale(void *state, short scale)
{
	*fld(state, FPM_TONE_OFF_SCALE) = scale;
}

void
FPM_TONE_generate(void *state, short *out, short count)
{
	struct fpm_phasor p;
	int scale = *fld(state, FPM_TONE_OFF_SCALE);
	int period = *fld(state, FPM_TONE_OFF_REV_PERIOD);
	int elapsed;
	int i;

	p.phase = (unsigned short)*fld(state, FPM_TONE_OFF_PHASE);
	p.inc = (unsigned short)*fld(state, FPM_TONE_OFF_INC);
	p.cos = p.sin = 0;

	for (i = 0; i < count; i++) {
		FPM_phasor(&p);
		out[i] = (short)((scale * p.sin) >> 14);
	}

	/*
	 * Phase-reversal bookkeeping.  The counter advances in units of eight
	 * samples, so at 8 kHz it ticks in milliseconds and the default period
	 * of 450 is the ITU-T V.25 figure directly.
	 */
	elapsed = (unsigned short)*fld(state, FPM_TONE_OFF_REV_COUNT)
		  + (count >> 3);

	if (period > 0 && period <= elapsed) {
		int phase = (short)p.phase;

		*fld(state, FPM_TONE_OFF_REV_COUNT) = 0;

		/*
		 * Half of a 0x8000 cycle is 180 degrees.  The original adds
		 * 0x4000 and, if that overflows the positive range, subtracts
		 * 0x4000 from the *original* phase instead -- which is the same
		 * rotation the other way, not a wrap.
		 */
		if (phase + 0x4000 > 0x7fff)
			phase = phase - 0x4000;
		else
			phase = phase + 0x4000;

		p.phase = (unsigned short)phase;
	} else {
		*fld(state, FPM_TONE_OFF_REV_COUNT) = (short)elapsed;
	}

	*fld(state, FPM_TONE_OFF_PHASE) = (short)p.phase;
}

/* Pointer-sized field access, for the slots that hold buffers. */
static void **
pfld(void *state, int off)
{
	return (void **)((char *)state + off);
}

/* The built-in configuration: the ITU-T V.25 answer tone. */


void *
FPM_TONE_create(void *state, const void *cfg)
{
	struct fpm_phasor p;
	int owned = 0;
	int increment;
	int damp;
	int len;
	int i;

	if (state == NULL) {
		state = sysdep_malloc(FPM_TONE_STATE_SIZE);
		if (state == NULL)
			return NULL;
		owned = 1;
	}

	sysdep_memcpy(state, cfg != NULL ? cfg : (const void *)FPM_TONE_CFG,
		      FPM_TONE_CFG_BYTES);

	len = *fld(state, FPM_TONE_CFG_LEN);

	/*
	 * Buffers belong to whoever allocated the object.  A caller supplying
	 * its own state supplies its own buffers, so this is skipped -- see
	 * the ownership contract in docs/findings.md.
	 */
	if (owned && len > 0) {
		*pfld(state, 0x2c) = sysdep_malloc((unsigned)len * 2);
		*pfld(state, 0x30) = sysdep_malloc(
			(unsigned)(len + *fld(state, FPM_TONE_CFG_EXTRA)) * 2);
		*pfld(state, 0xf4) = sysdep_malloc(10);
		*pfld(state, 0xf8) = sysdep_malloc(8);
	}

	/*
	 * Evaluate cos and sin *at* the phase increment, without advancing:
	 * phase = increment, inc = 0.  That gives cos(omega) for
	 * omega = 2*pi*f/8000, which is what the Goertzel coefficient needs.
	 */
	increment = ((int)*fld(state, FPM_TONE_CFG_FREQ) * 0x8312 + 0x1000) >> 13;
	p.phase = (unsigned short)increment;
	p.inc = 0;
	p.cos = p.sin = 0;
	FPM_phasor(&p);

	*fld(state, FPM_TONE_OFF_PHASE) = 0;
	*fld(state, FPM_TONE_OFF_REV_COUNT) = 0;
	*fld(state, FPM_TONE_OFF_INC) = (short)p.phase;

	damp = *fld(state, FPM_TONE_CFG_DAMP);

	/* Exact-frequency Goertzel section. */
	*fld(state, 0x36) = 0x4000;
	*fld(state, 0x38) = 0x4000;
	*fld(state, 0x3a) = (short)-(2 * p.cos);
	*fld(state, 0x3c) = (short)((damp * damp) >> 16);
	*fld(state, 0x3e) = (short)((-(p.cos * damp)) >> 14);
	for (i = 0x40; i <= 0x50; i += 2)
		*fld(state, i) = 0;
	*fld(state, 0x34) = 0;
	for (i = 0x52; i <= 0xf0; i += 2)
		*fld(state, i) = 0;

	/*
	 * Correlation reference: the source waveform modulated by a cosine
	 * sweeping at the tone frequency.  Setting inc to the current phase is
	 * what starts that sweep.
	 */
	p.inc = p.phase;
	{
		const short *src = (const short *)*pfld(state, FPM_TONE_CFG_SRC);
		short *ref = (short *)*pfld(state, 0x2c);
		short *zero = (short *)*pfld(state, 0x30);

		for (i = 0; i < len; i++) {
			FPM_phasor(&p);
			zero[i] = 0;
			ref[i] = (short)((2 * src[i] * p.cos) >> 14);
		}
	}

	/*
	 * Damped resonator, r = 0.96.  Evaluated from phase 0 so cos = 1.0;
	 * a caller retunes it later via FPM_TONE_set_freq.
	 */
	*pfld(state, 0xfc) = (char *)state + 0x36;
	*fld(state, 0x100) = 0;
	*fld(state, 0x102) = 0;
	*fld(state, 0x104) = 0;
	*fld(state, 0x106) = 0;

	p.phase = 0;
	p.inc = 0;
	FPM_phasor(&p);
	{
		short *blk = (short *)*pfld(state, 0xf4);
		short *acc = (short *)*pfld(state, 0xf8);

		blk[0] = 0x4000;
		blk[1] = 0x4000;
		blk[2] = (short)-(2 * p.cos);
		blk[3] = 0x3afb;			/* 0.96^2       */
		blk[4] = (short)((p.cos * -31457) >> 14);	/* -2 * 0.96 */

		acc[0] = acc[1] = acc[2] = acc[3] = 0;
	}

	return state;
}

/*
 * Tear down a tone object.
 *
 * Note the asymmetry with create, which is the original's and is reproduced.
 * create allocates the four buffers only when it allocated the object *and*
 * len > 0; delete frees them whenever len > 0, with no ownership test, and
 * then frees the object unconditionally.
 *
 * So a caller that supplied its own state with a positive len would have its
 * buffer slots freed without them ever having been allocated, and its state
 * freed even if it lived on the stack.  That never happens in practice --
 * every call site passes NULL to create -- but the mismatch is real, and
 * "tidying" delete to match create would change behaviour for the paths that
 * do exist.  See D5 in docs/deviations.md.
 */
void
FPM_TONE_delete(void *state)
{
	if (*fld(state, FPM_TONE_CFG_LEN) > 0) {
		sysdep_free(*pfld(state, 0xf8));
		sysdep_free(*pfld(state, 0xf4));
		sysdep_free(*pfld(state, 0x30));
		sysdep_free(*pfld(state, 0x2c));
	}
	sysdep_free(state);
}

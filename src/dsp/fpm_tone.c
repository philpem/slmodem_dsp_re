/*
 * fpm_tone.c -- tone generator and detector.
 *
 * Reconstructed from dsplibs.o fpm_tone.c:
 *   FPM_TONE_set_freq   .text 0x0aaf50
 *   FPM_TONE_set_scale  .text 0x0aaf70
 *   FPM_TONE_generate   .text 0x0aad50
 *
 * Not yet reconstructed: FPM_TONE_create/_delete and the detector half
 * (_detect, _find_rev, _filter, _kill, _generate2, _generate_demod).
 *
 * The object is 0x108 bytes and is treated here as opaque storage accessed at
 * known offsets, because most of its fields belong to the detector.  That is
 * deliberate: naming a struct now would mean guessing at fields whose meaning
 * has not been established, and a wrong layout is worse than none.
 */

#include "dsplib/fpm_tone.h"
#include "dsplib/fpm_phasor.h"

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

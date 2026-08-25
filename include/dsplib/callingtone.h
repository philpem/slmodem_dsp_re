/*
 * callingtone.h -- Calling Tone: the tone the caller sends while waiting.
 *
 * A calling tone tells whoever picks up that a machine is calling, so that a
 * fax or modem at the far end can answer automatically instead of a person
 * having to.  V.25 specifies 1300 Hz, 0.5 to 0.7 seconds on and 1.5 to 2.0
 * seconds off; fax CNG is 1100 Hz, 0.5 on and 3.0 off.
 *
 * CALLPROG_Progress emits it into the transmit buffer for the whole of the
 * dialling and waiting phase, if the caller enabled it.
 *
 * WHAT THIS ONE ACTUALLY PRODUCES
 *
 * Not 1300 Hz, and not a sine.  The constants are right for 1300 Hz at a
 * 9600 Hz sample rate, but call progress runs at a fixed 8000 (finding F41),
 * and the phase-to-table scaling is out by a factor of eight, so only the
 * first eighth of the cosine is ever swept.  The result is a mostly-DC
 * sawtooth with a repetition rate of 1084 Hz.
 *
 * The level parameter does not work either: over the whole range of the
 * signed char it takes, the amplitude moves between 0.93 and 1.08 of full
 * scale.
 *
 * All three are in the original; see docs/deviations.md D11 to D13, which also
 * record how far each one is from what was evidently meant.  Nothing is fixed
 * here: they degrade an optional courtesy signal rather than dropping a call.
 *
 * They are not unreachable, though.  CALLPROG_Dial enables the generator when
 * GetCallingToneFlag is 1 or 2, and of slmodemd's fifty country parameter sets
 * one -- CZECH_REPUBLIC -- ships 1.
 */

#ifndef DSPLIB_CALLINGTONE_H
#define DSPLIB_CALLINGTONE_H

/*
 * Cadence, in samples.  At the 8000 Hz call progress actually runs at these
 * are 0.72 s and 2.10 s; at the 9600 Hz they were computed for, 0.60 s and
 * 1.75 s, which is inside V.25's tolerance.
 */
#define CALLING_TONE_ON		0x1680		/* 5760 samples  */
#define CALLING_TONE_OFF	0x41a0		/* 16800 samples */

/*
 * Phase accumulator: 14 bits for one cycle, advanced by this each sample.
 * 2219/16384 of 9600 is 1300.2 Hz; of 8000 it is 1083.7 Hz.
 */
#define CALLING_TONE_STEP	0x8ab
#define CALLING_TONE_PHASE_MASK	0x3fff

struct calling_tone {
	short	phase;		/* +0x00  14-bit accumulator            */
	int	on;		/* +0x04  nonzero during the tone burst */
	int	remaining;	/* +0x08  samples left in this period   */
	short	amplitude;	/* +0x0c  Q14, from ResetCallingTone    */
};

/*
 * Start the cadence at the beginning of a tone burst and set the amplitude
 * from `level`.  `level` is a signed char -- see the header comment for why
 * it barely matters what you pass.
 */
void ResetCallingTone(struct calling_tone *ct, char level);

/*
 * Fill `count` samples with the tone or with silence, according to where the
 * cadence has got to, advancing it.  Writes every sample; there is no
 * mix-in.
 */
void GenerateCallingTone(struct calling_tone *ct, short *buf, int count);

#endif /* DSPLIB_CALLINGTONE_H */

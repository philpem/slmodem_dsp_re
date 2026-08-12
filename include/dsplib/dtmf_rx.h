/*
 * dtmf_rx.h -- the DTMF RECEIVER used by Caller ID, and its object.
 *
 * This is a different detector from `dtmf.h`'s and shares nothing with it.
 * That one is float, runs at 4 kHz off a bank of notches, and is reached
 * from `detector_progress`.  This one is fixed point, runs at the line rate,
 * and is reached from `cid_progress` -- DTMF-carried Caller ID, where the
 * calling number arrives as tone pairs between the rings.
 *
 * Three translation units of the original meet in this object:
 *
 *   Dtmf_Rx.c        reset_dtmf, create_cid_dtmf, band_pass, dtmf_modem
 *   Dtmf_Detector.c  DTMF_MTD_detect
 *
 * `band_pass` is put in Dtmf_Rx.c by the link order, not by its name: see
 * finding 1410 for the derivation, which also disposes of
 * docs/attribution.md's `Data.c|Dtmf.c` guess.
 *
 * HOW IT FITS TOGETHER.  `dtmf_modem` is the state machine.  It splits each
 * incoming block in half and asks `band_pass` -- which is both a filter and
 * an energy gate -- whether each half carries signal.  A silent half followed
 * by a loud one is a tone starting, and the machine then aligns its analysis
 * window to that edge by carrying half a block over in `hold`.  Once aligned
 * it hands whole blocks to `DTMF_MTD_detect`, which is the tone bank proper.
 * Digits accumulate as ASCII in `digits` until 'C' (index 12) terminates the
 * string.
 */

#ifndef DSPLIB_DTMF_RX_H
#define DSPLIB_DTMF_RX_H

/*
 * 0x38c bytes; `create_cid_dtmf` allocates exactly that.
 *
 * The two buffers are the bulk of it.  `samples` is what `bufp` points at
 * when the window has been realigned -- the second half of the PREVIOUS
 * block followed by the first half of this one -- and `hold` is where that
 * carried-over half lives between calls.
 */
struct dtmf_rx {
	short f000;		/* +0x000 cleared by reset_dtmf           */
	short pad_002[1];	/* +0x002                                 */
	int f004;		/* +0x004 cleared by reset_dtmf           */
	short f008;		/* +0x008 cleared by reset_dtmf           */
	short samples[300];	/* +0x00a the realigned analysis window    */
	short hold[100];	/* +0x262 the carried-over half block      */
	short last_digit;	/* +0x32a what the tone bank said last     */
	short stable;		/* +0x32c how long it has said the same    */
	short ndigits;		/* +0x32e digits in `digits`; -1 is a
				 *        distinct "not started" state     */
	short state;		/* +0x330 0 idle, 1 hunting, 2 aligned,
				 *        3 timed out                      */
	short aligned;		/* +0x332 `hold` holds a real half block   */
	short quiet;		/* +0x334 consecutive silent decisions     */
	short level;		/* +0x336 band_pass's energy memory        */
	short *bufp;		/* +0x338 what the tone bank is given      */
	short rate;		/* +0x33c 8000 or 9600                     */
	short sens;		/* +0x33e threshold trim, 2..4 lower it    */
	char digits[20];	/* +0x340 the collected string, ASCII      */
	short f354[2];		/* +0x354 cleared by reset_dtmf, unused    */
	short bp_state[2];	/* +0x358 band_pass's biquad               */
	short f35c[2];		/* +0x35c cleared by reset_dtmf, unused    */
	short pre_high[2];	/* +0x360 the high group's pre-notch       */
	short pre_low[2];	/* +0x364 the low group's pre-notch --
				 *        NOT cleared by reset_dtmf, D251  */
	short tone_state[8][2];	/* +0x368 one biquad per tone              */
	int nsamples;		/* +0x388 samples since the last decision  */
};

/* dtmf_modem's rate field, and DTMF_MTD_detect's. */
#define DTMF_RX_RATE_9600	9600

/*
 * What `create_cid_dtmf` puts there.  Nothing tests for it: `band_pass` and
 * the tone bank ask only whether the rate IS 9600, so 8000 is the rate by
 * being the other one.
 */
#define DTMF_RX_RATE_8000	8000

/*
 * Clear everything the state machine accumulates and arm it: `state` comes
 * out as 1 (hunting), `ndigits` as -1 ("not started", distinct from 0),
 * `last_digit` as -1, `level` as 1 and `bufp` as `rx->samples`.
 *
 * It does NOT touch `rate`, `sens`, `aligned`, `pre_low`, the two buffers, or
 * `digits[16..19]` -- the digit-clearing loop stops at 15 where the array is
 * 20 (D302), and `pre_low` is D251.  So it is a reset of the RECEIVER, not of
 * the object: the configuration a caller put in survives it, which is what
 * `cid_reset` relies on.
 */
void reset_dtmf(struct dtmf_rx *rx);

/*
 * Build a Caller ID DTMF receiver.  `rx` NULL allocates one, otherwise the
 * caller's storage is used; either way the object is returned, which is what
 * `cid_create` stores back over the pointer it passed in.
 *
 * Sets `rate` to 8000 and `sens` to 0 and then resets.  There is no rate or
 * sensitivity argument -- a caller wanting 9600 or a trimmed threshold writes
 * the field itself afterwards.
 */
struct dtmf_rx *create_cid_dtmf(struct dtmf_rx *rx);

/*
 * The tone bank.  `count` samples in, one of the sixteen keypad codes out --
 * 0..9, 10 'A', 11 'B', 12 'C', 13 'D', 14 '*', 15 '#' -- or -9 for "no
 * agreement".  The eight resonator states and the two pre-notches live in
 * `rx` and persist across calls; the energies do not.
 */
int DTMF_MTD_detect(const short *samples, short count, struct dtmf_rx *rx);

/*
 * Filter `count` samples IN PLACE and say whether they carry signal.
 *
 * It is three things at once, which is why it is 1,187 bytes: a limiter
 * that divides down a block that is clipping, a DC blocker, a coarse gain
 * that multiplies a quiet block up by 6, 3 or 2, and only then the bandpass
 * the name refers to.  Returns 1 for signal, 0 for silence.
 */
int band_pass(short *samples, short count, struct dtmf_rx *rx);

/*
 * One block of line samples.  Returns 1 while nothing has happened, 2 once
 * digits are arriving, 3 when a complete string has been terminated by 'C',
 * and -1 on any of the several give-up conditions.
 *
 * `count` MUST be 198 or less: the block is copied into a stack array of
 * that size with no bound check, exactly as the object does it (D252).
 */
int dtmf_modem(const short *samples, unsigned short count,
	       struct dtmf_rx *rx);

/*
 * The tone bank's coefficients: one FPM_iir_filt section each, five shorts,
 * a notch at the tone's own frequency with its poles at radius 0.9.  MTD1..8
 * are 697 770 852 941 1209 1336 1477 1633 Hz.  See docs/coefficients.md --
 * and note MTD7_COEF_9600, which does not fit the design (D250).
 */
extern const short MTD1_COEF_8000[5], MTD1_COEF_9600[5];
extern const short MTD2_COEF_8000[5], MTD2_COEF_9600[5];
extern const short MTD3_COEF_8000[5], MTD3_COEF_9600[5];
extern const short MTD4_COEF_8000[5], MTD4_COEF_9600[5];
extern const short MTD5_COEF_8000[5], MTD5_COEF_9600[5];
extern const short MTD6_COEF_8000[5], MTD6_COEF_9600[5];
extern const short MTD7_COEF_8000[5], MTD7_COEF_9600[5];
extern const short MTD8_COEF_8000[5], MTD8_COEF_9600[5];

#endif /* DSPLIB_DTMF_RX_H */

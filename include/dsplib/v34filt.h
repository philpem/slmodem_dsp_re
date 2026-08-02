/*
 * v34filt.h -- ITU-T V.34: the DSP layer under the handshake (v34filters.c).
 *
 * The only V.34 translation unit with no state machine in it.  Everything
 * here is a filter, an adaptive filter, or a piece of bookkeeping for one:
 * the echo canceller, the Hilbert transformer that makes the received signal
 * analytic, the symbol-timing recovery filters, the adaptive equaliser, and
 * the transmit modulator.
 *
 * WHY EVERY OBJECT HERE IS DECLARED STANDALONE.  All of them are *members* of
 * the enclosing V.34 object -- the receiver forms each argument with a `lea`
 * or an `add` off one base register, so the echo canceller is at +0x80b8, its
 * twin at +0x9138, the equaliser at +0x630 and the modulator at +0x1450.  But
 * every one is passed by pointer and touches nothing outside itself, so the
 * parent's layout is not needed to reconstruct them and they are declared as
 * their own types.  When V34RX.c arrives it will embed these, not redefine
 * them.
 *
 * TWO ECHO CANCELLERS.  V34RX.c keeps a pair and chooses between them on a
 * flag at +0xa23c -- one for each of the two paths a V.34 receiver has to
 * cancel.  They are the same type and this file cannot tell them apart.
 *
 * See the standing caveat at the top of v34det.h about what tier-1 testing
 * can and cannot establish for V.34, and about the hardware peer that will
 * change that.
 */

#ifndef DSPLIB_V34FILT_H
#define DSPLIB_V34FILT_H

#ifdef __cplusplus
extern "C" {
#endif

/* ------------------------------------------------------------------------
 * The echo canceller
 */

/*
 * One echo canceller, 0x20 bytes, and every field but one is a pointer or a
 * length -- the arrays live outside it.
 *
 * The coefficients are held at DOUBLE WIDTH, split across two arrays: `coeff`
 * carries the high 16 bits of each tap and `coeff_frac` the low 16.  The
 * filter reads only the high half, and the adaptation carries the full 32
 * bits, so a tap can accumulate a correction far smaller than one LSB of the
 * filter's own resolution and still eventually move it.  That is the whole
 * reason the second array exists.
 */
struct v34_echo {
	short *cursor;		/* +0x00  write position in `dline`      */
	short *dline;		/* +0x04  circular, `dlen` shorts        */
	short *coeff;		/* +0x08  high half, `taps` shorts       */
	short *coeff_frac;	/* +0x0c  low half                       */
	short *hist;		/* +0x10  tap history, `taps` shorts     */
	void *unused_14;	/* +0x14  no reader or writer found      */
	unsigned dlen;		/* +0x18  delay line length, in shorts   */
	unsigned taps;		/* +0x1c                                 */
};

/*
 * Reset.
 *
 * Rewinds the cursor and zeroes the coefficients and the tap history -- and
 * NOT the delay line, which keeps whatever it held.  See docs/deviations.md;
 * that is the original's behaviour and it is reproduced.
 */
void V34EchoCleanUp(struct v34_echo *e);

/* Push one sample into the circular delay line. */
void V34EchoUpdateDelayLine(struct v34_echo *e, short sample);

/*
 * Convolve, and return the 32-bit accumulator unshifted -- the caller
 * chooses the output scaling.
 *
 * `lag` selects how far behind the cursor the newest tap is taken from.
 */
int V34EchoFilter(struct v34_echo *e, short lag);

/* One LMS step: every tap moves by `err` times its own history entry. */
void V34EchoAdapt(struct v34_echo *e, short err);

/* Sum of the tap history squared, scaled down by 32. */
int V34EchoEstimateDelayLineEnergy(struct v34_echo *e);

/* Log the coefficients, if logging is on.  See the note in v34filters.c. */
void V34EchoReportCoeff(struct v34_echo *e);

/* ------------------------------------------------------------------------
 * The Hilbert transformer
 */

#define V34_HILBERT_TAPS	64

/*
 * The 64-tap pair that turns a real signal into an analytic one.  Global in
 * the object; both are Q15 and neither is symmetric, because a Hilbert pair
 * is a filter and its quadrature partner rather than one filter twice.
 */
extern const short V34hilbertrealcoef[V34_HILBERT_TAPS];
extern const short V34hilbertimagcoef[V34_HILBERT_TAPS];

/* Zero the 64-entry state.  Takes the state array, not an object. */
void V34InitHilbertFilter(short *state);

/*
 * One sample in, a complex pair out, both as unshifted 32-bit accumulators.
 *
 * The caller does the rounding: V34RX.c adds 0x2000 and shifts by 14.
 */
void V34HilbertFilter(short *state, short sample, int *re, int *im);

/* ------------------------------------------------------------------------
 * Symbol timing recovery
 */

#define V34_TIMING_HP_TAPS	40

/*
 * `V34TimingHPFilterCoeff` is Q16, not Q15 -- V34TimingHPFilter rounds with
 * 0x8000 and shifts by 16, where everything else in this subtree uses 0x2000
 * and 14.
 */
extern const short V34TimingHPFilterCoeff[V34_TIMING_HP_TAPS];
/*
 * V34TimingPrefilterCoeff, V34TimingIIR_Acoef and V34TimingIIR_Bcoef are the
 * rest of the set and are declared with the functions that read them, once
 * V34TimingFilter and V34TimingPrefilter are reconstructed.
 */

/*
 * The timing object, mapped only as far as the high-pass needs.
 *
 * V34TimingHPFilter is handed the whole object and reaches its delay line at
 * +0x24, so the argument is not the array.  The rest of the struct belongs to
 * V34TimingFilter and V34TimingFiltersInit and is a pad until those land; the
 * offset is held by an assertion in v34filters.c meanwhile.
 */
struct v34_timing {
	unsigned char unmapped_00[0x24];
	short hp_hist[V34_TIMING_HP_TAPS];	/* +0x24 */
};

/* The 40-tap high-pass ahead of the timing recovery. */
int V34TimingHPFilter(struct v34_timing *t, short sample);

/* ------------------------------------------------------------------------
 * The adaptive equaliser
 */

#define V34_EQ_TAPS		80
#define V34_EQ_CENTRE_FIRST	36	/* first tap CenterAdapt touches   */
#define V34_EQ_CENTRE_TAPS	8
#define V34_EQ_UNITY		0x1000	/* what CleanUp puts in tap 40     */

/*
 * The equaliser: an 80-tap complex FIR with a circular delay line, 0x3cc
 * bytes, entirely self-contained.
 *
 * The coefficients use the same double-width trick as the echo canceller --
 * `re`/`im` are the high halves and `re_frac`/`im_frac` the low ones -- but
 * only `V34EqualizerAdapt` honours it.  `V34EqualizerCenterAdapt` writes the
 * high halves only, with rounding, and leaves the fractional halves stale.
 * See docs/findings.md; that is a real asymmetry and not a reconstruction
 * artefact.
 */
struct v34_equalizer {
	short dly_re[V34_EQ_TAPS];	/* +0x000 */
	short dly_im[V34_EQ_TAPS];	/* +0x0a0 */
	short re[V34_EQ_TAPS];		/* +0x140 */
	short im[V34_EQ_TAPS];		/* +0x1e0 */
	short re_frac[V34_EQ_TAPS];	/* +0x280 */
	short im_frac[V34_EQ_TAPS];	/* +0x320 */
	int cursor;			/* +0x3c0 */
	/*
	 * Eight bytes CleanUp's memset covers and nothing here reads or
	 * writes.  Sized from that memset -- 0x3cc -- rather than assumed,
	 * which is the only evidence for the object's extent there is.
	 */
	int reserved_3c4[2];		/* +0x3c4 */
};

/* Zero everything, then set tap 40 to unity -- a flat initial response. */
void V34EqualizerCleanUp(struct v34_equalizer *q);

/* Zero taps 36..43 of both the real and imaginary coefficient arrays. */
void V34EqualizerClearCenterTaps(struct v34_equalizer *q);

/* Push one complex sample into the delay line. */
void V34EqualizerUpdateDelayLine(struct v34_equalizer *q, short re, short im);

/* Convolve; both outputs are unshifted 32-bit accumulators. */
void V34EqualizerFilter(struct v34_equalizer *q, int *re, int *im);

/* Complex LMS over every tap, at 32-bit precision.  dc = -e * conj(d). */
void V34EqualizerAdapt(struct v34_equalizer *q, short err_re, short err_im);

/*
 * The same gradient over the 8 centre taps only, at 16-bit precision with
 * round-to-nearest.  NOT a subset of V34EqualizerAdapt -- see the header
 * comment on the struct.
 */
void V34EqualizerCenterAdapt(struct v34_equalizer *q, short err_re,
			     short err_im);

/* ------------------------------------------------------------------------
 * Odds and ends
 */

/*
 * Install a pointer, and nothing else.  Both are one store.
 *
 * `V34EchoPreFilterCopy` writes at +0x54 of its first argument and
 * `V34PremptxCopy` at +0xcb0, which is the modulator's pre-emphasis slot --
 * the same field V34SetupModulator fills with `preemp0`.  Neither offset can
 * be given a type until the objects around them are reconstructed, so both
 * take `void *` and say so.
 */
void V34EchoPreFilterCopy(void *dst, const short *coeff);
void V34PremptxCopy(void *modulator, const short *coeff);

#ifdef __cplusplus
}
#endif

#endif /* DSPLIB_V34FILT_H */

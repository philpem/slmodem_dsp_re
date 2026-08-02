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
 * The sizes V34InitializeImplementationSpecific installs, for both cancellers
 * (finding 98).  Named here because three other things are derived from
 * them: the report dump length, D27's wrap bound, and the storage layout in
 * struct v34_object.
 */
#define V34_ECHO_TAPS	144	/* 0x90  */
#define V34_ECHO_DLEN	1656	/* 0x678 */

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
	/*
	 * +0x0c, the low half -- and also what DPSK.c's fskdetect
	 * dereferences as its delay line.  The two share the array and are
	 * never live together; see docs/findings.md, 100.
	 */
	short *coeff_frac;
	short *hist;		/* +0x10  tap history, `taps` shorts     */
	/*
	 * +0x14.  Recorded here for a long time as having no reader or
	 * writer; adaptecho has one.  It steps this as a SHORT, once per
	 * call, and nothing else found so far touches it -- so the upper
	 * half of the word stays whatever the allocation left, and the
	 * counter wraps every 65536 symbols with no reader to care.
	 */
	short adapt_count;	/* +0x14                                 */
	unsigned char pad_16[2];/* +0x16                                 */
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

/*
 * Point both echo cancellers at their storage inside the V.34 object, and
 * set their two lengths.
 *
 * Declared with `void *` rather than `struct v34_object *` to keep v34filt.h
 * free of v34fsk.h -- the dependency runs the other way, because the object
 * owns the cancellers' storage.  v34filters.c casts once, at the top.
 */
void V34InitializeImplementationSpecific(void *obj);

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

/*
 * Zero the 64-entry state, and RETURN IT.
 *
 * The original is a tail call to sysdep_memset, so it leaves the destination
 * in the return register.  Declared `void *` because a caller relies on it:
 * `rxinit` stores the low half into a receiver field (D34).  Reproducing
 * that needs the value, so the signature says what the object does rather
 * than what the source probably said.
 */
void *V34InitHilbertFilter(short *state);

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
#define V34_TIMING_PRE_TAPS	40
/*
 * How much of the object V34TimingFiltersInit actually zeroes past `iir`:
 * eighty SHORTS, which is the high-pass history plus the first twenty
 * entries of the prefilter state.  See docs/deviations.md, D29.
 */
#define V34_TIMING_INIT_SHORTS	80

/*
 * `V34TimingHPFilterCoeff` is Q16, not Q15 -- V34TimingHPFilter rounds with
 * 0x8000 and shifts by 16, where everything else in this subtree uses 0x2000
 * and 14.
 */
extern const short V34TimingHPFilterCoeff[V34_TIMING_HP_TAPS];
extern const short V34TimingPrefilterCoeff[40];

/*
 * The half-baud band-pass pair, and the fourth-order real filter that is the
 * same thing in another form.
 *
 * V.34 recovers symbol timing by watching the two spectral lines a modulated
 * signal shows at plus and minus half the baud rate.  These are the filters
 * that isolate them: `pos` and `neg` are complex second-order sections
 * differing ONLY in the sign of their imaginary denominator half, which makes
 * them one filter and its mirror image about DC.
 *
 * Their poles sit at +/-1/8 of the sample rate -- +/-45 degrees, measured at
 * 0.98 radius -- and that is why there is one set rather than five.  An
 * eighth of the sample rate is half the baud rate exactly when the timing
 * path runs at four samples per symbol, so the filters are rate-independent
 * and the resampler ahead of them carries the rate.  The object's own C++
 * side names that resampler: `ResamplerTiming::adjustHalfBaudBpfGain`.
 *
 * `V34TimingIIR_Acoef`/`Bcoef` are a fourth-order REAL filter whose four
 * poles land on the same two frequencies, positive and negative -- the
 * cascade of the two complex sections written out as one real one.  See
 * docs/coefficients.md for the numbers.
 */
extern const short negHalfBaud_Acoef_Imag[3];
extern const short negHalfBaud_Acoef_Real[3];
extern const short negHalfBaud_Bcoef_Imag[3];
extern const short negHalfBaud_Bcoef_Real[3];
extern const short posHalfBaud_Acoef_Imag[3];
extern const short posHalfBaud_Acoef_Real[3];
extern const short posHalfBaud_Bcoef_Imag[3];
extern const short posHalfBaud_Bcoef_Real[3];
extern const short V34TimingIIR_Acoef[5];
extern const short V34TimingIIR_Bcoef[5];

/*
 * The timing object, mapped only as far as the high-pass needs.
 *
 * V34TimingHPFilter is handed the whole object and reaches its history at
 * +0x24, so the argument is not the array.  What is named here is what
 * V34TimingFiltersInit touches; the gap between is a pad until
 * V34TimingFilter lands, and every named offset is held by an assertion in
 * v34filters.c meanwhile.
 */
struct v34_timing {
	/*
	 * Six three-entry groups, zeroed by V34TimingFiltersInit as a 3-outer
	 * 6-inner nest striding by six shorts -- which is what a `short[6][3]`
	 * looks like once the compiler has transposed the loops.  The IIR
	 * coefficient pair below is five entries, so these are most likely its
	 * state; V34TimingFilter will settle it.
	 */
	short iir[6][3];			/* +0x000 */
	short hist[V34_TIMING_HP_TAPS];		/* +0x024, to +0x073 */
	/*
	 * The prefilter's state: forty COMPLEX entries, each packed into one
	 * int as (im << 16) | (unsigned short)re.  It abuts the coefficient
	 * pointer below exactly, which is what fixes both its length and the
	 * high-pass history's above it.
	 */
	int pre_state[V34_TIMING_PRE_TAPS];	/* +0x074, to +0x113 */
	const short *prefilter_coeff;		/* +0x114 */
	const short *hp_coeff;			/* +0x118 */
	int in0;				/* +0x11c  newest complex pair */
	int in1;				/* +0x120  and the one before  */
};

/*
 * Zero the state and install the two coefficient pointers.
 *
 * Note that V34TimingHPFilter does NOT read `hp_coeff`: it addresses
 * V34TimingHPFilterCoeff directly.  The pointer is installed for whoever
 * else needs it, presumably V34TimingFilter.
 */
void V34TimingFiltersInit(struct v34_timing *t);

int V34TimingFilter(struct v34_timing *t, int sample);

/* The 40-tap high-pass ahead of the timing recovery. */
int V34TimingHPFilter(struct v34_timing *t, short sample);

/*
 * The 40-tap complex prefilter.
 *
 * Takes no sample: it reads the two newest complex inputs out of `in0` and
 * `in1` and returns the result packed the same way, (im << 16) | re.
 */
int V34TimingPrefilter(struct v34_timing *t);

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
 * The transmit modulator
 */

#define V34_MOD_ROW	64	/* destination taps per polyphase row */

/*
 * The modulator, mapped where V34SetupModulator writes it.  A member of the
 * V.34 object at +0x1450; the pads are not a claim about their contents.
 *
 * The ten `hsine*` tables hold `sine_len` sine values followed by `sine_len`
 * cosine values -- two contiguous halves, not interleaved.  `sine_len` is
 * therefore each table's byte size / 4.
 */
struct v34_modulator {
	int taps;		/* +0x00  source taps per polyphase row  */
	int f04;		/* +0x04                                 */
	int rows;		/* +0x08  polyphase rows to load         */
	int row;		/* +0x0c  polyphase row, mod `rows`      */
	const short *sine;	/* +0x10  carrier table                  */
	int sine_len;		/* +0x14  its length, in complex pairs   */
	int phase;		/* +0x18  carrier phase, mod `sine_len`  */
	unsigned char unmapped_1c[0xc24 - 0x1c];
	short *shaped;		/* +0xc24  where the engine writes       */
	unsigned char unmapped_c28[0xc7c - 0xc28];
	const short *ec_prem;	/* +0xc7c                                */
	unsigned char unmapped_c80[0xc8c - 0xc80];
	int fc8c;		/* +0xc8c  an integer, 14 or 15          */
	short prem_hist[16];	/* +0xc90  pre-emphasis history          */
	const short *preemp;	/* +0xcb0                                */
	unsigned char unmapped_cb4[0xcbc - 0xcb4];
	unsigned char work_cbc[0x100];		/* memset 0x100 from +0xcbc */
	unsigned char unmapped_dbc[0xdbc - (0xcbc + 0x100)];
	int wpos;		/* +0xdbc  write index into work_cbc     */
	int wstep;		/* +0xdc0  offset to the imaginary half  */
};

extern const short hsine1200[16], hsine1600[12], hsine1680[80];
extern const short hsine1800[32], hsine1829[42], hsine1867[72];
extern const short hsine1920[10], hsine1959[98], hsine2000[48];
extern const short hsine2400[16];

/*
 * Configure the modulator for one (baud, carrier) pair.
 *
 * `baud` selects the shaping filter and the three counts; `carrier` selects
 * the sine table AND the pre-emphasis variant; `phase` picks a row of the
 * p<baud> table when non-zero; `reset` non-zero clears the working state.
 */
void V34SetupModulator(struct v34_modulator *m, short baud, short carrier,
		       short phase, int arg4, int reset);

/*
 * Modulate one complex symbol into `out`, returning how many samples it
 * produced.  The symbol arrives packed as (im << 16) | (unsigned short)re.
 */
int V34ModulatorProcess(struct v34_modulator *m, int symbol, short *out);

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

#define V34_ECHO_PREFILTER_TAPS	42

/*
 * The echo path's pre-filter, mapped as far as its two users need.
 *
 * `coeff` is the field V34EchoPreFilterCopy installs into, which is what
 * attributes that one-line function to this object rather than to another.
 * The three fields at +0x58, +0x5c and +0x60 are read by
 * V34EchoHistoryBackwardClean and are named when that lands.
 */
struct v34_echo_prefilter {
	short state[V34_ECHO_PREFILTER_TAPS];	/* +0x00 */
	const short *coeff;			/* +0x54 */
	short hist_pos;				/* +0x58  index into `state` */
	short pad_5a;
	int hist_len;				/* +0x5c  entries in `state` */
	int span;				/* +0x60  halved, see below  */
	int shift;				/* +0x64 */
};

/*
 * Filter `count` samples in place through a 42-tap FIR, rounding with 0x4000
 * and shifting by `p->shift`.
 */
void V34EchoPreFilter(short *buf, short count, struct v34_echo_prefilter *p);

/*
 * Roll the echo canceller's history back by `n` samples.
 *
 * Takes the whole V.34 object, not a canceller: it clears the pre-filter's
 * circular history, walks BOTH cancellers' delay lines backwards, and zeroes
 * three fixed scratch areas.  Declared `void *` for the same reason
 * V34InitializeImplementationSpecific is.
 */
void V34EchoHistoryBackwardClean(void *obj, unsigned n);

/*
 * A generic FIR: one sample in, `taps` of state shifted, the raw accumulator
 * out.
 *
 * NOTHING IN THE OBJECT CALLS IT.  It is the general form of a loop that
 * V34TimingHPFilter, V34EchoPreFilter and the DPSK interpolator each write
 * out by hand, which is a plausible reason for it to have been written and
 * then not used.  Reproduced for the same reason as `cosread` -- see
 * docs/findings.md.
 */
int V34Filter2(short sample, short *state, const short *coeff, unsigned taps);

#ifdef __cplusplus
}
#endif

#endif /* DSPLIB_V34FILT_H */

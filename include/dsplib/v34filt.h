/*
 * v34filt.h -- ITU-T V.34: the DSP layer under the handshake (v34filters.c).
 *
 * The only V.34 translation unit with no state machine in it.  Everything
 * here is a filter, an adaptive filter, or a piece of bookkeeping for one:
 * the echo canceller, the Hilbert transformer that makes the received signal
 * analytic, the symbol-timing recovery filters, the adaptive equaliser, and
 * the transmit modulator.
 *
 * Why every object here is declared standalone: all of them are *members* of
 * the enclosing V.34 object -- the receiver forms each argument with a `lea`
 * or an `add` off one base register, so the echo canceller, its twin, the
 * equaliser and the modulator each live at a fixed offset inside it. But
 * every one is passed by pointer and touches nothing outside itself, so the
 * parent's layout is not needed to reconstruct them and they are declared as
 * their own types. `struct v34_object` (v34fsk.h) is that parent, and embeds
 * `struct v34_echo` and `struct v34_echo_prefilter` directly (finding F98);
 * the equaliser is reached from `struct v34_receiver` (v34recv.h) by a
 * documented offset instead, to keep that header's dependency one-way.
 *
 * V34RX.c keeps a pair of echo cancellers and chooses between them on a
 * flag -- one for each of the two paths a V.34 receiver has to cancel.
 * They are the same type and this file cannot tell them apart.
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

/**
 * The tap count and delay-line length V34InitializeImplementationSpecific
 * installs for both echo cancellers (finding F98). Named here because three
 * other things are derived from them: the report dump length, D27's wrap
 * bound, and the storage layout in `struct v34_object`.
 */
#define V34_ECHO_TAPS	144	/* 0x90  */
#define V34_ECHO_DLEN	1656	/* 0x678 */

/**
 * @brief One echo canceller, 0x20 bytes; every field but one is a pointer
 * or a length, the arrays themselves living outside it.
 *
 * The coefficients are held at double width, split across two arrays:
 * `coeff` carries the high 16 bits of each tap and `coeff_frac` the low 16.
 * The filter reads only the high half; the adaptation carries the full 32
 * bits, so a tap can accumulate a correction far smaller than one LSB of the
 * filter's own resolution and still eventually move it.
 */
struct v34_echo {
	short *cursor;		/* +0x00  write position in `dline`      */
	short *dline;		/* +0x04  circular, `dlen` shorts        */
	short *coeff;		/* +0x08  high half, `taps` shorts       */
	/*
	 * +0x0c, the low half -- also what DPSK.c's fskdetect dereferences
	 * as its delay line; the two share the array and are never live
	 * together (finding F100).
	 */
	short *coeff_frac;
	short *hist;		/* +0x10  tap history, `taps` shorts     */
	/*
	 * Incremented once per call by adaptecho(); no reader has been found
	 * for it, so it free-wraps every 65536 symbols.
	 */
	short adapt_count;	/* +0x14                                 */
	unsigned dlen;		/* +0x18  delay line length, in shorts.
				 * pad_16[2] removed here -- pure alignment
				 * gap ahead of this `unsigned`; `adapt_count`
				 * ends on a 2-mod-4 offset.  Confirmed by the
				 * assertion below and a disassembly search of
				 * every function touching this struct (base
				 * at v34_object+0x80b8): nothing reads or
				 * writes absolute offset 0x80ce/0x80cf
				 * (finding F10149). */
	unsigned taps;		/* +0x1c                                 */
};

#if defined(__SIZEOF_POINTER__) && __SIZEOF_POINTER__ == 4
typedef char v34echo_off_dlen[
	((int)__builtin_offsetof(struct v34_echo, dlen) == 0x18) ? 1 : -1];
typedef char v34echo_size2[(sizeof(struct v34_echo) == 0x20) ? 1 : -1];
#endif

/**
 * @brief Reset one echo canceller.
 *
 * Rewinds the cursor and zeroes the coefficients and the tap history --
 * but not the delay line, which keeps whatever it held. That is the
 * original's behaviour, reproduced as-is (see docs/deviations.md).
 *
 * @param e  The echo canceller to reset.
 */
void V34EchoCleanUp(struct v34_echo *e);

/**
 * @brief Push one sample into the circular delay line.
 * @param e       The echo canceller.
 * @param sample  The sample to enqueue.
 */
void V34EchoUpdateDelayLine(struct v34_echo *e, short sample);

/**
 * @brief Convolve the canceller against its delay line.
 *
 * The caller chooses the output scaling; the accumulator is returned
 * unshifted.
 *
 * @param e    The echo canceller.
 * @param lag  How far behind the cursor the newest tap is taken from.
 * @return The unshifted 32-bit convolution result.
 */
int V34EchoFilter(struct v34_echo *e, short lag);

/**
 * @brief One LMS adaptation step.
 *
 * Every tap moves by @p err times its own history entry.
 *
 * @param e    The echo canceller.
 * @param err  The error signal driving adaptation.
 */
void V34EchoAdapt(struct v34_echo *e, short err);

/**
 * @brief Estimate the delay line's signal energy.
 * @param e  The echo canceller.
 * @return The tap history's sum of squares, scaled down by 32.
 */
int V34EchoEstimateDelayLineEnergy(struct v34_echo *e);

/**
 * @brief Log the canceller's coefficients, if logging is enabled.
 * @param e  The echo canceller.
 */
void V34EchoReportCoeff(struct v34_echo *e);

/**
 * @brief Point both echo cancellers at their storage inside the V.34
 * object, and set their lengths.
 *
 * @param obj  The V.34 modem object. Declared `void *` rather than
 *             `struct v34_object *` to keep v34filt.h free of v34fsk.h --
 *             the dependency runs the other way, since the object owns
 *             the cancellers' storage. v34filters.c casts once, at the top.
 */
void V34InitializeImplementationSpecific(void *obj);

/* ------------------------------------------------------------------------
 * The Hilbert transformer
 */

#define V34_HILBERT_TAPS	64

/**
 * The 64-tap pair that turns a real signal into an analytic one. Global in
 * the object; both are Q15 and neither is symmetric, because a Hilbert
 * pair is a filter and its quadrature partner rather than one filter
 * twice.
 */
extern const short V34hilbertrealcoef[V34_HILBERT_TAPS];
extern const short V34hilbertimagcoef[V34_HILBERT_TAPS];

/**
 * @brief Zero a Hilbert filter's state.
 *
 * Returns the (now-zeroed) state pointer: the original is a tail call to
 * `sysdep_memset`, which leaves its destination in the return register, so
 * the signature reflects what the object does rather than what the source
 * probably said. The one call site in this reconstruction (`rxinit`)
 * discards the value -- an earlier reading of that call as relying on it
 * was wrong and is retracted (deviation D34).
 *
 * @param state  The 64-entry Hilbert filter state to zero.
 * @return @p state.
 */
void *V34InitHilbertFilter(short *state);

/**
 * @brief Run one sample through the Hilbert transformer.
 *
 * @param state   The filter state.
 * @param sample  The input sample.
 * @param re      Output: real part, unshifted 32-bit accumulator.
 * @param im      Output: imaginary part, unshifted 32-bit accumulator.
 *                The caller does the rounding (V34RX.c adds 0x2000 and
 *                shifts by 14).
 */
void V34HilbertFilter(short *state, short sample, int *re, int *im);

/* ------------------------------------------------------------------------
 * Symbol timing recovery
 */

#define V34_TIMING_HP_TAPS	40
#define V34_TIMING_PRE_TAPS	40
/**
 * How much of the object V34TimingFiltersInit() actually zeroes past `iir`:
 * eighty shorts, which covers the high-pass history but only the first
 * twenty entries of the prefilter state (deviation D29).
 */
#define V34_TIMING_INIT_SHORTS	80

/**
 * `V34TimingHPFilterCoeff` is Q16, not Q15 -- V34TimingHPFilter() rounds
 * with 0x8000 and shifts by 16, where everything else in this subtree uses
 * 0x2000 and 14.
 */
extern const short V34TimingHPFilterCoeff[V34_TIMING_HP_TAPS];
extern const short V34TimingPrefilterCoeff[40];

/**
 * The half-baud band-pass pair, and the fourth-order real filter that is
 * the same thing in another form.
 *
 * V.34 recovers symbol timing by watching the two spectral lines a
 * modulated signal shows at plus and minus half the baud rate. These are
 * the filters that isolate them: `pos` and `neg` are complex second-order
 * sections differing only in the sign of their imaginary denominator half,
 * which makes them one filter and its mirror image about DC.
 *
 * Their poles sit at +/-1/8 of the sample rate -- +/-45 degrees, measured
 * at 0.98 radius -- which is why there is one set rather than five: an
 * eighth of the sample rate is half the baud rate exactly when the timing
 * path runs at four samples per symbol, so the filters are rate-independent
 * and the resampler ahead of them carries the rate. The object's own C++
 * side names that resampler: `ResamplerTiming::adjustHalfBaudBpfGain`.
 *
 * `V34TimingIIR_Acoef`/`Bcoef` are a fourth-order real filter whose four
 * poles land on the same two frequencies, positive and negative -- the
 * cascade of the two complex sections written out as one real one. See
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

/**
 * @brief Symbol-timing recovery state: the two half-baud band-pass filters'
 * IIR history, the 40-tap complex prefilter's state, and the two most
 * recent complex input samples.
 *
 * Fully mapped -- every field is one of V34TimingFiltersInit(),
 * V34TimingHPFilter(), V34TimingPrefilter() or V34TimingFilter() actually
 * touches, with no unmodelled gap.
 */
struct v34_timing {
	/*
	 * Six three-entry state histories for the two half-baud band-pass
	 * filters: iir[0]/iir[1] hold the shared numerator input history (the
	 * down-converted carrier, real and imaginary), and iir[2]/iir[3] and
	 * iir[4]/iir[5] hold the positive and negative filter's own output
	 * (denominator) history respectively. Zeroed by V34TimingFiltersInit
	 * as a 3-outer, 6-inner nest striding by six shorts -- the transposed
	 * form of a `short[6][3]` that a compiler produces from a source loop
	 * ordered the other way.
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

/**
 * @brief Zero a timing-recovery object's state and install its two
 * coefficient pointers.
 *
 * Neither V34TimingHPFilter() nor V34TimingFilter() actually reads
 * `hp_coeff` or `prefilter_coeff` back: both address
 * `V34TimingHPFilterCoeff`/`V34TimingPrefilterCoeff` directly. The pointers
 * are installed and never consulted by anything in this file.
 *
 * @param t  The timing-recovery object to initialise.
 */
void V34TimingFiltersInit(struct v34_timing *t);

/**
 * @brief The symbol-timing recovery step.
 * @param t       The timing-recovery object.
 * @param sample  The input sample.
 * @return The recovered timing metric.
 */
int V34TimingFilter(struct v34_timing *t, int sample);

/**
 * @brief The 40-tap high-pass filter ahead of timing recovery.
 * @param t       The timing-recovery object.
 * @param sample  The input sample.
 * @return The filtered output.
 */
int V34TimingHPFilter(struct v34_timing *t, short sample);

/**
 * @brief The 40-tap complex prefilter.
 *
 * Takes no sample argument: reads the two newest complex inputs out of
 * `in0` and `in1`.
 *
 * @param t  The timing-recovery object.
 * @return The filtered complex output, packed as `(im << 16) | re`.
 */
int V34TimingPrefilter(struct v34_timing *t);

/* ------------------------------------------------------------------------
 * The adaptive equaliser
 */

#define V34_EQ_TAPS		80
#define V34_EQ_CENTRE_FIRST	36	/* first tap CenterAdapt touches   */
#define V34_EQ_CENTRE_TAPS	8
#define V34_EQ_UNITY		0x1000	/* what CleanUp puts in tap 40     */

/**
 * @brief The V.34 adaptive equaliser: an 80-tap complex FIR with a
 * circular delay line, 0x3cc bytes, entirely self-contained.
 *
 * The coefficients are held at double width, the same trick as the echo
 * canceller: `re`/`im` are the high halves and `re_frac`/`im_frac` the low
 * ones. Only V34EqualizerAdapt() honours the fractional halves --
 * V34EqualizerCenterAdapt() writes the high halves only, with rounding,
 * and leaves the fractional halves stale. That is a real asymmetry
 * between the two adapters, not a reconstruction artefact (finding F95).
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

/**
 * @brief Reset the equaliser to a flat initial response.
 *
 * Zeroes everything, then sets tap 40 to unity.
 *
 * @param q  The equaliser.
 */
void V34EqualizerCleanUp(struct v34_equalizer *q);

/**
 * @brief Zero the equaliser's centre taps.
 *
 * Clears taps 36..43 of both the real and imaginary coefficient arrays.
 *
 * @param q  The equaliser.
 */
void V34EqualizerClearCenterTaps(struct v34_equalizer *q);

/**
 * @brief Push one complex sample into the equaliser's delay line.
 * @param q   The equaliser.
 * @param re  Real part of the sample.
 * @param im  Imaginary part of the sample.
 */
void V34EqualizerUpdateDelayLine(struct v34_equalizer *q, short re, short im);

/**
 * @brief Convolve the equaliser against its delay line.
 * @param q   The equaliser.
 * @param re  Output: real part, unshifted 32-bit accumulator.
 * @param im  Output: imaginary part, unshifted 32-bit accumulator.
 */
void V34EqualizerFilter(struct v34_equalizer *q, int *re, int *im);

/**
 * @brief Complex LMS adaptation over every tap, at 32-bit precision.
 *
 * `dc = -e * conj(d)`.
 *
 * @param q       The equaliser.
 * @param err_re  Real part of the error signal.
 * @param err_im  Imaginary part of the error signal.
 */
void V34EqualizerAdapt(struct v34_equalizer *q, short err_re, short err_im);

/**
 * @brief The same gradient as V34EqualizerAdapt(), over the 8 centre taps
 * only, at 16-bit precision with round-to-nearest.
 *
 * Not a subset of V34EqualizerAdapt(): it writes the high coefficient
 * halves only, with rounding, and leaves the fractional halves stale (see
 * the struct v34_equalizer documentation).
 *
 * @param q       The equaliser.
 * @param err_re  Real part of the error signal.
 * @param err_im  Imaginary part of the error signal.
 */
void V34EqualizerCenterAdapt(struct v34_equalizer *q, short err_re,
			     short err_im);

/* ------------------------------------------------------------------------
 * The transmit modulator
 */

#define V34_MOD_ROW	64	/* destination taps per polyphase row */

/**
 * @brief The V.34 transmit modulator, mapped as far as
 * V34SetupModulator() writes it. A member of the V.34 object; the
 * `unmapped_*` gaps are not a claim about their contents.
 *
 * The ten `hsine*` tables hold `sine_len` sine values followed by
 * `sine_len` cosine values -- two contiguous halves, not interleaved.
 * `sine_len` is therefore each table's byte size / 4.
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

/**
 * @brief Configure the modulator for one (baud, carrier) pair.
 *
 * The entry diagnostic names all five parameters: "baudrate %ld, carrier
 * %ld, preemp %ld, V90=%ld. fullReset=%1d" (finding F172).
 *
 * @param m             The modulator to configure.
 * @param baud          Selects the shaping filter and the three sample counts.
 * @param carrier       Selects the sine table and the pre-emphasis variant.
 * @param preemp_index  Picks a row of the `p<baud>` table, when non-zero.
 * @param v90           Selects the V.90 shaping/pre-emphasis pair, but only
 *                      at 3200 baud -- every other rate just prints it
 *                      (finding F216, superseding F172's "read by nothing").
 * @param reset         Non-zero clears the modulator's working state.
 */
void V34SetupModulator(struct v34_modulator *m, short baud, short carrier,
		       short preemp_index, int v90, int reset);

/**
 * @brief Modulate one complex symbol.
 * @param m       The modulator.
 * @param symbol  The complex symbol, packed as `(im << 16) | (unsigned short)re`.
 * @param out     Output sample buffer.
 * @return Number of samples written to @p out.
 */
int V34ModulatorProcess(struct v34_modulator *m, int symbol, short *out);

/* ------------------------------------------------------------------------
 * Odds and ends
 */

/**
 * @brief Install the echo pre-filter's coefficient pointer.
 *
 * One store: writes into the pre-filter's `coeff` field. The target
 * object cannot yet be given a real type (see struct v34_echo_prefilter),
 * so the argument stays `void *`.
 *
 * @param dst    The echo pre-filter object.
 * @param coeff  The coefficient table to install.
 */
void V34EchoPreFilterCopy(void *dst, const short *coeff);

/**
 * @brief Install the transmit modulator's pre-emphasis coefficient pointer.
 *
 * One store: writes into the modulator's pre-emphasis slot (the same
 * field V34SetupModulator() fills with `preemp0`). The target object
 * cannot yet be given a real type (see struct v34_modulator), so the
 * argument stays `void *`.
 *
 * @param modulator  The transmit modulator object.
 * @param coeff      The coefficient table to install.
 */
void V34PremptxCopy(void *modulator, const short *coeff);

#define V34_ECHO_PREFILTER_TAPS	42

/**
 * @brief The echo path's pre-filter: a 42-tap FIR plus the circular
 * history window used to roll it back after a line hit.
 *
 * `coeff` is the field V34EchoPreFilterCopy() installs -- that call is what
 * attributes it to this object rather than another. `hist_pos`, `hist_len`
 * and `span` are read by V34EchoHistoryBackwardClean().
 */
struct v34_echo_prefilter {
	short state[V34_ECHO_PREFILTER_TAPS];	/* +0x00 */
	const short *coeff;			/* +0x54 */
	short hist_pos;				/* +0x58  index into `state` */
	int hist_len;				/* +0x5c  entries in `state`.
						 * pad_5a[2] removed here --
						 * pure alignment gap ahead of
						 * this `int`; `hist_pos` ends
						 * on a 2-mod-4 offset.
						 * Confirmed by the assertion
						 * below and a disassembly
						 * search of every function
						 * touching this struct (base
						 * at v34_object+0x2078):
						 * nothing reads or writes
						 * absolute offset
						 * 0x20d2/0x20d3 (finding
						 * F10149). */
	int span;				/* +0x60  halved, see below  */
	int shift;				/* +0x64 */
};

#if defined(__SIZEOF_POINTER__) && __SIZEOF_POINTER__ == 4
typedef char v34echopf_off_histlen[
	((int)__builtin_offsetof(struct v34_echo_prefilter, hist_len)
		== 0x5c) ? 1 : -1];
typedef char v34echopf_size[
	(sizeof(struct v34_echo_prefilter) == 0x68) ? 1 : -1];
#endif

/**
 * @brief Filter samples in place through the echo path's 42-tap FIR.
 * @param buf    Samples to filter, in place.
 * @param count  Number of samples in @p buf.
 * @param p      The echo pre-filter; rounds with 0x4000 and shifts by
 *               `p->shift`.
 */
void V34EchoPreFilter(short *buf, short count, struct v34_echo_prefilter *p);

/**
 * @brief Roll the echo cancellers' history back by @p n samples.
 *
 * Takes the whole V.34 object, not a single canceller: clears the
 * pre-filter's circular history, walks both cancellers' delay lines
 * backwards, and zeroes three fixed scratch areas.
 *
 * @param obj  The V.34 modem object. Declared `void *` for the same
 *             reason V34InitializeImplementationSpecific() is.
 * @param n    Number of samples to roll back.
 */
void V34EchoHistoryBackwardClean(void *obj, unsigned n);

/**
 * @brief A generic FIR: one sample in, `taps` of state shifted, the raw
 * accumulator out.
 *
 * Nothing in the object calls it -- it is the general form of a loop that
 * V34TimingHPFilter(), V34EchoPreFilter() and the DPSK interpolator each
 * write out by hand instead, a plausible reason for it to exist unused.
 * Reproduced anyway, for the same reason as `cosread` (see docs/findings.md,
 * finding F89, for that case): an exported symbol with no caller is a fact
 * about the original interface, not something this reconstruction gets to
 * decide was a mistake.
 *
 * @param sample  The input sample.
 * @param state   Filter state, `taps` entries, shifted in place.
 * @param coeff   Filter coefficients, `taps` entries.
 * @param taps    Number of taps.
 * @return The unshifted 32-bit accumulator.
 */
int V34Filter2(short sample, short *state, const short *coeff, unsigned taps);

#ifdef __cplusplus
}
#endif

#endif /* DSPLIB_V34FILT_H */

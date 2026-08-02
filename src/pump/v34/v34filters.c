/*
 * v34filters.c -- ITU-T V.34: the DSP layer under the handshake.
 *
 * The only V.34 translation unit with no state machine in it, and the one
 * everything above it stands on: the echo canceller, the Hilbert transformer,
 * the symbol-timing filters, the adaptive equaliser and the modulator.
 *
 * This file is being reconstructed in pieces.  What is here so far is the
 * echo canceller, the Hilbert transformer, the timing high-pass, and the
 * odds and ends; the equaliser, the remaining timing filters and the
 * modulator follow.  The functions are written in the object's own order so
 * that a diff against the disassembly stays readable.
 *
 * THE ECHO CANCELLER IS THE INTERESTING ONE.  It is an ordinary LMS
 * adaptive FIR with one unusual property: its coefficients are 32 bits wide,
 * held as two parallel arrays of shorts.  `V34EchoFilter` reads only the high
 * halves, so the filter runs at 16-bit resolution, while `V34EchoAdapt`
 * carries all 32 bits.  A correction far below the filter's own least
 * significant bit therefore accumulates in the low array until it carries
 * into the high one -- which is how a canceller with 16-bit taps converges to
 * better than 16-bit accuracy.  Nothing in the object comments on it and it
 * is easy to mistake the second array for a scratch buffer.
 *
 * SCALING IS THE CALLER'S JOB HERE, unlike everywhere else in this
 * reconstruction.  `V34EchoFilter` and `V34HilbertFilter` both hand back
 * unshifted 32-bit accumulators and V34RX.c does the rounding -- with 0x2000
 * and a shift of 14 in both cases.  `V34TimingHPFilter` is the exception that
 * proves it: it rounds internally, with 0x8000 and a shift of 16, because its
 * coefficients are Q16 rather than Q15.
 *
 * See the standing caveat at the top of v34det.h.
 */

#include "dsplib/debug.h"
#include "dsplib/sysdep.h"
#include "dsplib/v34filt.h"

/*
 * The Hilbert transform pair, 64 taps each, Q15.
 *
 * Global in the object, at .rodata 0x3440 and 0x34c0, immediately before
 * v34filters.c's own static block -- which is what attributes them here,
 * since a translation unit's globals precede its locals (findings 78).
 *
 * Neither is symmetric and neither is the other reversed: they are a filter
 * and its quadrature partner, which is the point.  Emitted as data; a
 * derivation has not been attempted yet and docs/coefficients.md says so.
 */
const short V34hilbertrealcoef[V34_HILBERT_TAPS] = {
	    45,    -88,     25,    -58,    -90,     21,   -202,      8,
	  -174,   -170,    -38,   -370,    -12,   -358,   -254,   -123,
	  -588,      8,   -619,   -291,   -213,   -864,    180,  -1047,
	  -146,   -283,  -1406,   1195,  -2920,   2395,  12857,   2395,
	 -2920,   1195,  -1406,   -283,   -146,  -1047,    180,   -864,
	  -213,   -291,   -619,      8,   -588,   -123,   -254,   -358,
	   -12,   -370,    -38,   -170,   -174,      8,   -202,     21,
	   -90,    -58,     25,    -88,     45,      0,      0,      0,
};

const short V34hilbertimagcoef[V34_HILBERT_TAPS] = {
	    63,     63,     -7,    147,    -29,    130,     67,      1,
	   199,    -89,    178,     11,    -48,    209,   -261,    200,
	  -184,   -170,    118,   -618,    171,   -643,   -439,   -157,
	 -1483,    100,  -2105,  -1320,  -1561,  -9560,      0,   9560,
	  1561,   1320,   2105,   -100,   1483,    157,    439,    643,
	  -171,    618,   -118,    170,    184,   -200,    261,   -209,
	    48,    -11,   -178,     89,   -199,     -1,    -67,   -130,
	    29,   -147,      7,    -63,    -63,      0,      0,      0,
};

/*
 * The timing high-pass, 40 taps, Q16.
 *
 * Q16 and not Q15: V34TimingHPFilter rounds with 0x8000 and shifts by 16.
 * That is the only place in this subtree that does, and it means these
 * coefficients cannot be compared like-for-like against the Hilbert pair
 * above.
 */
const short V34TimingHPFilterCoeff[V34_TIMING_HP_TAPS] = {
	     3,      9,    -27,    -34,     87,     75,   -195,   -128,
	   368,    187,   -634,   -247,   1048,    301,  -1751,   -344,
	  3267,    373, -10358,  16001, -10358,    373,   3267,   -344,
	 -1751,    301,   1048,   -247,   -634,    187,    368,   -128,
	  -195,     75,     87,    -34,    -27,      9,      3,      0,
};

/*
 * The timing prefilter, 40 taps.  Installed by V34TimingFiltersInit and read
 * by V34TimingFilter, which is not reconstructed yet -- so its Q format is
 * not yet established.  See finding 94 on why that cannot be read off the
 * table.
 */
const short V34TimingPrefilterCoeff[40] = {
	     1,     -5,     -5,      8,     32,     40,      1,    -84,
	  -150,   -102,     94,    337,    406,    115,   -481,  -1014,
	  -958,     23,   1779,   3649,   4789,   4653,   3322,   1450,
	  -137,   -899,   -819,   -293,    195,    371,    252,     32,
	  -111,   -121,    -53,     12,     34,     21,      4,     -4,
};

/* ------------------------------------------------------------ echo canceller */

void
V34EchoCleanUp(struct v34_echo *e)
{
	unsigned i;

	e->cursor = e->dline;

	/*
	 * The coefficients, both halves, and the tap history.
	 *
	 * NOT the delay line.  `dline` keeps whatever it held, which under the
	 * harness's fill is the fill pattern and on a real modem is the tail
	 * of the previous connection.  It is not obviously harmless -- the
	 * next V34EchoFilter convolves fresh coefficients against stale
	 * samples -- but the coefficients are zero at that point, so the
	 * first output is zero regardless and the line has been overwritten
	 * by the time they are not.  Registered as D26 and reproduced.
	 */
	for (i = 0; i < e->taps; i++) {
		e->coeff[i] = 0;
		e->coeff_frac[i] = 0;
		e->hist[i] = 0;
	}

	if (DSPLIB_DEBUG_ON())
		dsplibs_debug_printf("V34EchoCleanUp\n");
}

void
V34EchoUpdateDelayLine(struct v34_echo *e, short sample)
{
	short *p = e->cursor;

	*p = sample;
	p++;
	if (p >= e->dline + e->dlen)
		p = e->dline;
	e->cursor = p;
}

int
V34EchoFilter(struct v34_echo *e, short lag)
{
	unsigned taps = e->taps;
	short *hist = e->hist;
	const short *p;
	int acc = 0;
	unsigned k;

	/*
	 * Shift the history down by one, discarding the oldest.  Skipped
	 * entirely when there is only one tap, which is the original's own
	 * special case rather than an optimisation added here.
	 */
	if (taps != 1)
		for (k = 0; k < taps - 1; k++)
			hist[k] = hist[k + 1];

	/*
	 * The newest entry comes from `lag + taps` shorts past the cursor,
	 * wrapped once.
	 *
	 * ONCE is the word.  The original subtracts the delay line's length a
	 * single time, so an index more than one length past the end stays
	 * past the end and the read leaves the buffer.  Whether that is
	 * reachable depends on the range of `lag` at the two call sites in
	 * V34RX.c, which is not yet reconstructed; see docs/deviations.md,
	 * D27.  Reproduced, because a second wrap would be a different
	 * function and no differential test could say which the caller wants.
	 */
	p = e->cursor + lag + taps - 1;
	if (p >= e->dline + e->dlen)
		p -= e->dlen;
	hist[taps - 1] = *p;

	for (k = 0; k < taps; k++)
		acc += e->coeff[k] * hist[k];

	return acc;
}

void
V34EchoAdapt(struct v34_echo *e, short err)
{
	unsigned k;

	/*
	 * The 32-bit tap is reassembled from its two halves, moved, and split
	 * again.  `coeff_frac` is read as UNSIGNED and `coeff` as signed,
	 * which is what makes the pair a single two's-complement 32-bit value
	 * rather than two independent numbers.
	 */
	for (k = 0; k < e->taps; k++) {
		int tap = (int)((unsigned)e->coeff[k] << 16)
			  + (unsigned short)e->coeff_frac[k];

		tap = (int)((unsigned)tap + (unsigned)(e->hist[k] * err));

		e->coeff[k] = (short)(tap >> 16);
		e->coeff_frac[k] = (short)tap;
	}
}

int
V34EchoEstimateDelayLineEnergy(struct v34_echo *e)
{
	int acc = 0;
	unsigned k;

	/*
	 * Named for the delay line and computed over the tap HISTORY, which
	 * is a different array.  The name is the original's; the arithmetic is
	 * `e->hist`, unambiguously, at +0x10.
	 */
	for (k = 0; k < e->taps; k++)
		acc = (int)((unsigned)acc
			    + (unsigned)((e->hist[k] * e->hist[k]) >> 5));

	return acc;
}

/*
 * How many coefficients V34EchoReportCoeff prints, regardless of `taps`.
 * See the note in the function.
 */
#define V34_ECHO_REPORT_TAPS	144
#define V34_ECHO_REPORT_COLS	6

void
V34EchoReportCoeff(struct v34_echo *e)
{
	unsigned n = (e->taps / V34_ECHO_REPORT_COLS) * V34_ECHO_REPORT_COLS;
	unsigned k;
	int i;

	/* Nothing to say if every tap the scan covers is still zero. */
	for (k = 0; k < n; k++)
		if (e->coeff[k] != 0)
			break;

	if (k == n) {
		if (DSPLIB_DEBUG_ON())
			dsplibs_debug_printf("Echo coefficients all zero\n");
		return;
	}

	if (!DSPLIB_DEBUG_ON())
		return;

	dsplibs_debug_printf("Echo coefficients:\n");

	/*
	 * THE SCAN AND THE DUMP DISAGREE ABOUT HOW LONG THE ARRAY IS.  The
	 * scan above stops at `taps` rounded down to a multiple of six; this
	 * loop runs to a hardcoded 144 whatever `taps` says, so a canceller
	 * with fewer than 144 taps has its coefficient array over-read here.
	 *
	 * Reproduced.  It is a logging path gated on a level slmodemd ships
	 * at zero, so it cannot fire on a working modem, and the read is of
	 * the reconstruction's own allocation rather than of anything the
	 * caller owns.  D28.
	 */
	for (i = 0; i <= V34_ECHO_REPORT_TAPS - V34_ECHO_REPORT_COLS;
	     i += V34_ECHO_REPORT_COLS) {
		if (!DSPLIB_DEBUG_ON())
			return;
		dsplibs_debug_printf("%d %d %d %d %d %d\n",
				     e->coeff[i], e->coeff[i + 1],
				     e->coeff[i + 2], e->coeff[i + 3],
				     e->coeff[i + 4], e->coeff[i + 5]);
	}
}

/* ------------------------------------------------------- Hilbert transformer */

void
V34InitHilbertFilter(short *state)
{
	sysdep_memset(state, 0, V34_HILBERT_TAPS * sizeof(short));
}

void
V34HilbertFilter(short *state, short sample, int *re, int *im)
{
	int carry = 0;
	int acc_re = 0;
	int acc_im = 0;
	int k;

	state[0] = sample;

	/*
	 * Filter and shift in one pass: each entry is read, then overwritten
	 * with the one before it.  `state[0]` is left holding zero on the way
	 * past, which does not matter because the next call writes the new
	 * sample there before anything reads it.
	 */
	for (k = 0; k < V34_HILBERT_TAPS; k++) {
		int x = state[k];

		state[k] = (short)carry;
		acc_re += V34hilbertrealcoef[k] * x;
		acc_im += V34hilbertimagcoef[k] * x;
		carry = x;
	}

	*re = acc_re;
	*im = acc_im;
}

/* ------------------------------------------------------------ timing recovery */

int
V34TimingHPFilter(struct v34_timing *t, short sample)
{
	short *hist = t->hist;
	int carry = sample;
	int acc = 0x8000;		/* Q16 round-to-nearest */
	int k;

	/*
	 * Same read-then-overwrite shift as the Hilbert filter, with one
	 * difference that matters: the product uses the value being WRITTEN,
	 * not the one being read.  So tap k multiplies x[n-k] rather than
	 * x[n-1-k], and the filter has no extra sample of delay in it.
	 */
	for (k = 0; k < V34_TIMING_HP_TAPS; k++) {
		int old = hist[k];

		hist[k] = (short)carry;
		acc = (int)((unsigned)acc
			    + (unsigned)(carry * V34TimingHPFilterCoeff[k]));
		carry = old;
	}

	return acc >> 16;
}

/* --------------------------------------------------------------- equaliser */

void
V34EqualizerCleanUp(struct v34_equalizer *q)
{
	sysdep_memset(q, 0, sizeof(*q));

	/*
	 * A flat initial response: one unity tap in the middle and nothing
	 * else.  0x1000 rather than 0x4000 or 0x7fff, so the coefficients are
	 * Q12 here even though the delay line they multiply is a raw sample.
	 */
	q->re[V34_EQ_TAPS / 2] = V34_EQ_UNITY;
}

void
V34EqualizerClearCenterTaps(struct v34_equalizer *q)
{
	int k;

	/*
	 * Taps 36..43, which includes the unity tap CleanUp installs at 40.
	 * The fractional halves are NOT cleared, which matters because
	 * V34EqualizerAdapt will read them back.
	 */
	for (k = V34_EQ_CENTRE_FIRST;
	     k < V34_EQ_CENTRE_FIRST + V34_EQ_CENTRE_TAPS; k++) {
		q->re[k] = 0;
		q->im[k] = 0;
	}
}

void
V34EqualizerUpdateDelayLine(struct v34_equalizer *q, short re, short im)
{
	int c = q->cursor;

	q->dly_re[c] = re;
	q->dly_im[c] = im;

	c++;
	q->cursor = (c == V34_EQ_TAPS) ? 0 : c;
}

void
V34EqualizerFilter(struct v34_equalizer *q, int *re, int *im)
{
	int acc_re = 0;
	int acc_im = 0;
	int k = 0;
	int j;

	/*
	 * Two loops rather than one modulo: from the cursor to the end of the
	 * line, then from the start back to the cursor.  The coefficient
	 * index runs straight through both, so tap 0 always multiplies the
	 * OLDEST sample.
	 */
	for (j = q->cursor; j < V34_EQ_TAPS; j++, k++) {
		acc_re += q->dly_re[j] * q->re[k] - q->dly_im[j] * q->im[k];
		acc_im += q->dly_im[j] * q->re[k] + q->dly_re[j] * q->im[k];
	}
	for (j = 0; j < q->cursor; j++, k++) {
		acc_re += q->dly_re[j] * q->re[k] - q->dly_im[j] * q->im[k];
		acc_im += q->dly_im[j] * q->re[k] + q->dly_re[j] * q->im[k];
	}

	*re = acc_re;
	*im = acc_im;
}

/*
 * One tap of the complex LMS gradient, at full 32-bit width.
 *
 * The update is dc = -e * conj(d), spelled out: the real part loses
 * dre*ere + dim*eim and the imaginary part loses dre*eim while gaining
 * dim*ere.
 */
static void
eq_adapt_tap(short *hi, short *lo, int dre, int dim, int ere, int eim,
	     int conj)
{
	int tap = (int)((unsigned)*hi << 16) + (unsigned short)*lo;

	if (conj)
		tap = (int)((unsigned)tap - (unsigned)(dre * eim)
			    + (unsigned)(dim * ere));
	else
		tap = (int)((unsigned)tap - (unsigned)(dre * ere)
			    - (unsigned)(dim * eim));

	*hi = (short)(tap >> 16);
	*lo = (short)tap;
}

void
V34EqualizerAdapt(struct v34_equalizer *q, short err_re, short err_im)
{
	int k = 0;
	int j;

	for (j = q->cursor; j < V34_EQ_TAPS; j++, k++) {
		eq_adapt_tap(&q->re[k], &q->re_frac[k], q->dly_re[j],
			     q->dly_im[j], err_re, err_im, 0);
		eq_adapt_tap(&q->im[k], &q->im_frac[k], q->dly_re[j],
			     q->dly_im[j], err_re, err_im, 1);
	}
	for (j = 0; j < q->cursor; j++, k++) {
		eq_adapt_tap(&q->re[k], &q->re_frac[k], q->dly_re[j],
			     q->dly_im[j], err_re, err_im, 0);
		eq_adapt_tap(&q->im[k], &q->im_frac[k], q->dly_re[j],
			     q->dly_im[j], err_re, err_im, 1);
	}
}

void
V34EqualizerCenterAdapt(struct v34_equalizer *q, short err_re, short err_im)
{
	int n;

	/*
	 * The same gradient as V34EqualizerAdapt over the 8 centre taps, and
	 * NOT the same arithmetic:
	 *
	 *   - the tap is assembled from its high half alone, so whatever
	 *     V34EqualizerAdapt accumulated in the fractional half is
	 *     discarded on the way in and left stale on the way out;
	 *   - the result is rounded with 0x8000 rather than truncated.
	 *
	 * Read as a design that is plausible -- a fast coarse pull on the
	 * centre taps during acquisition, a fine 32-bit one everywhere
	 * afterwards -- but the two do fight over the same eight taps if both
	 * run, and nothing in this file arbitrates.  See docs/findings.md.
	 */
	for (n = 0; n < V34_EQ_CENTRE_TAPS; n++) {
		int k = V34_EQ_CENTRE_FIRST + n;
		int j = q->cursor + V34_EQ_CENTRE_FIRST + n;
		int dre, dim, tap;

		if (j >= V34_EQ_TAPS)
			j -= V34_EQ_TAPS;
		dre = q->dly_re[j];
		dim = q->dly_im[j];

		tap = (int)((unsigned)((unsigned)q->re[k] << 16)
			    - (unsigned)(dre * err_re)
			    - (unsigned)(dim * err_im) + 0x8000u);
		q->re[k] = (short)(tap >> 16);

		tap = (int)((unsigned)((unsigned)q->im[k] << 16)
			    - (unsigned)(dre * err_im)
			    + (unsigned)(dim * err_re) + 0x8000u);
		q->im[k] = (short)(tap >> 16);
	}
}

/* ------------------------------------------------------------ odds and ends */

void
V34TimingFiltersInit(struct v34_timing *t)
{
	int i, j;

	/*
	 * The original walks this as three outer steps of one short each,
	 * writing six entries six shorts apart -- the transposed form of a
	 * `short[6][3]`, which is why the type is two-dimensional here.
	 */
	for (i = 0; i < 6; i++)
		for (j = 0; j < 3; j++)
			t->iir[i][j] = 0;

	for (i = 0; i < 80; i++)
		t->hist[i] = 0;

	t->prefilter_coeff = V34TimingPrefilterCoeff;
	t->hp_coeff = V34TimingHPFilterCoeff;
}

int
V34Filter2(short sample, short *state, const short *coeff, unsigned taps)
{
	int carry = sample;
	int acc = 0;
	unsigned k;

	/*
	 * `taps` is unsigned -- the original's loop guard is `jb`, so a count
	 * of zero does nothing rather than running four billion times.
	 */
	for (k = 0; k < taps; k++) {
		int old = state[k];

		state[k] = (short)carry;
		acc = (int)((unsigned)acc + (unsigned)(carry * coeff[k]));
		carry = old;
	}

	return acc;
}

void
V34EchoPreFilter(short *buf, short count, struct v34_echo_prefilter *p)
{
	short i;

	for (i = 0; i < count; i++) {
		const short *coeff = p->coeff;
		unsigned shift = (unsigned char)p->shift;
		int carry = buf[i];
		int acc = 0;
		int k;

		/*
		 * Same read-then-overwrite shift as V34TimingHPFilter and
		 * V34Filter2, with the tap count fixed at 42 rather than
		 * passed in.
		 */
		for (k = 0; k < V34_ECHO_PREFILTER_TAPS; k++) {
			int old = p->state[k];

			p->state[k] = (short)carry;
			acc = (int)((unsigned)acc
				    + (unsigned)(carry * coeff[k]));
			carry = old;
		}

		/*
		 * In place: the input array is the output array.  `shift` is
		 * read as a byte and masked to five bits for the same reason
		 * as dftenergy's -- the object's `sar %cl` does the masking
		 * and C would otherwise be undefined.  Re-read every sample,
		 * as the original does, rather than hoisted.
		 */
		buf[i] = (short)((acc + 0x4000) >> (shift & 31));
	}
}


void
V34EchoPreFilterCopy(void *dst, const short *coeff)
{
	*(const short **)((char *)dst + 0x54) = coeff;
}

void
V34PremptxCopy(void *modulator, const short *coeff)
{
	*(const short **)((char *)modulator + 0xcb0) = coeff;
}

/*
 * ---------------------------------------------------------------------------
 * Layout, pinned.  Guarded to a 32-bit ABI: struct v34_echo is all pointers.
 */
#if defined(__SIZEOF_POINTER__) && __SIZEOF_POINTER__ == 4

#define V34F_ASSERT(name, type, field, off) \
	typedef char v34f_off_##name[ \
		((int)__builtin_offsetof(type, field) == (off)) ? 1 : -1]

V34F_ASSERT(cursor,     struct v34_echo, cursor,     0x00);
V34F_ASSERT(dline,      struct v34_echo, dline,      0x04);
V34F_ASSERT(coeff,      struct v34_echo, coeff,      0x08);
V34F_ASSERT(coeff_frac, struct v34_echo, coeff_frac, 0x0c);
V34F_ASSERT(hist,       struct v34_echo, hist,       0x10);
V34F_ASSERT(unused14,   struct v34_echo, unused_14,  0x14);
V34F_ASSERT(dlen,       struct v34_echo, dlen,       0x18);
V34F_ASSERT(taps,       struct v34_echo, taps,       0x1c);
typedef char v34f_echo_size[(sizeof(struct v34_echo) == 0x20) ? 1 : -1];

V34F_ASSERT(t_hist,     struct v34_timing, hist,      0x024);
V34F_ASSERT(t_pre,      struct v34_timing, prefilter_coeff, 0x114);
V34F_ASSERT(t_hp,       struct v34_timing, hp_coeff,  0x118);
V34F_ASSERT(pf_coeff,   struct v34_echo_prefilter, coeff, 0x54);
V34F_ASSERT(pf_shift,   struct v34_echo_prefilter, shift, 0x64);

V34F_ASSERT(eq_dly_re,  struct v34_equalizer, dly_re,  0x000);
V34F_ASSERT(eq_dly_im,  struct v34_equalizer, dly_im,  0x0a0);
V34F_ASSERT(eq_re,      struct v34_equalizer, re,      0x140);
V34F_ASSERT(eq_im,      struct v34_equalizer, im,      0x1e0);
V34F_ASSERT(eq_re_frac, struct v34_equalizer, re_frac, 0x280);
V34F_ASSERT(eq_im_frac, struct v34_equalizer, im_frac, 0x320);
V34F_ASSERT(eq_cursor,  struct v34_equalizer, cursor,  0x3c0);
typedef char v34f_eq_size[(sizeof(struct v34_equalizer) == 0x3cc) ? 1 : -1];

/* The unity tap CleanUp installs really is the one ClearCenterTaps clears. */
typedef char v34f_eq_centre[
	(V34_EQ_TAPS / 2 >= V34_EQ_CENTRE_FIRST
	 && V34_EQ_TAPS / 2 < V34_EQ_CENTRE_FIRST + V34_EQ_CENTRE_TAPS)
	? 1 : -1];

#endif

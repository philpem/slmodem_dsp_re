/*
 * t_v23filt.c -- differential test of V.23's four global coefficient tables.
 *
 * Tables are the one part of a reconstruction that cannot be checked by
 * reasoning about it.  A wrong coefficient produces a filter that still
 * filters, so nothing downstream crashes or obviously misbehaves; it just
 * demodulates slightly worse, and the failure surfaces a phase later as "the
 * receiver does not lock" with no obvious cause.  So compare every word.
 *
 * Byte equality is necessary but on its own it is a weak test -- it passes
 * just as happily on two tables of zeroes.  Three further groups guard that:
 *
 *   - structural: the properties the header CLAIMS about each table.  If
 *     _V23_MRF_FILT is symmetric and V23_IIR_FILT has b0 == b2 per section,
 *     those are checkable statements, and they are exactly the statements the
 *     coefficient ORDER was deduced from.  Getting the order wrong is the
 *     realistic mistake here, not mistyping a digit.
 *   - functional: run the reference filter engines over both copies and
 *     compare the output, so the tables are proved equivalent in the use the
 *     object actually puts them to, not just in memory.
 *   - anti-vacuity: that the filters do something.  A degenerate table would
 *     pass every other check in this file.
 */

#include <string.h>

#include "harness.h"
#include "dsplib/v23fp.h"
#include "dsplib/fpm_iir.h"

extern const short ref__V23_MRF_FILT[];
extern const short ref__V23RX_ANSWER_INTRP[];
extern const short ref__V23RX_IIR_LPF[];
extern const short ref_V23_IIR_FILT[];

extern short ref_FPM_iir_filt(short x, const short *coeff, short *state,
			      short sections);
extern void ref_FPM_iir_filt_II(short *samples, const short *coeff,
				short *state, short sections, short count);

/* Coverage: set when a filtered block is proved to be a real signal. */
static int saw_signal;

static void
compare(const char *what, const short *ours, const short *ref, int n)
{
	int i;

	for (i = 0; i < n; i++)
		diff_eq_int(what, ours[i], ref[i], i);
}

/*
 * A block with something at every frequency the filters care about, so a
 * wrong coefficient cannot hide in a band the stimulus never excites.  A
 * plain impulse would test only the first few taps before the state runs dry.
 */
static void
stimulus(short *buf, int n)
{
	int i;
	long x = 0x1234567;

	buf[0] = 16384;
	for (i = 1; i < n; i++) {
		x = x * 1103515245 + 12345;
		buf[i] = (short)((x >> 16) & 0x3fff) - 0x2000;
	}
}

#define BLOCK 160

/* Run FPM_iir_filt_II over both copies of a table and compare every sample. */
static void
filter_ii(const char *what, const short *ours, const short *ref, int sections)
{
	short a[BLOCK], b[BLOCK];
	short sa[4 * 8], sb[4 * 8];
	int i;
	int nonzero = 0;

	stimulus(a, BLOCK);
	memcpy(b, a, sizeof(b));
	memset(sa, 0, sizeof(sa));
	memset(sb, 0, sizeof(sb));

	ref_FPM_iir_filt_II(a, ours, sa, (short)sections, BLOCK);
	ref_FPM_iir_filt_II(b, ref, sb, (short)sections, BLOCK);

	for (i = 0; i < BLOCK; i++) {
		diff_eq_int(what, a[i], b[i], i);
		if (a[i] != 0)
			nonzero++;
	}
	for (i = 0; i < 4 * sections; i++)
		diff_eq_int("iir_II state", sa[i], sb[i], i);

	/* Not "output != input": a passthrough would satisfy that. */
	diff_eq_int("%s: the filter produced a signal", nonzero > BLOCK / 2, 1,
		    0);
	if (nonzero > BLOCK / 2)
		saw_signal++;
}

/* The same for the direct-form-II engine, which takes one sample at a time. */
static void
filter_i(const char *what, const short *ours, const short *ref, int sections)
{
	short in[BLOCK];
	short sa[2 * 8], sb[2 * 8];
	int i;
	int nonzero = 0;

	stimulus(in, BLOCK);
	memset(sa, 0, sizeof(sa));
	memset(sb, 0, sizeof(sb));

	for (i = 0; i < BLOCK; i++) {
		short ya = ref_FPM_iir_filt(in[i], ours, sa, (short)sections);
		short yb = ref_FPM_iir_filt(in[i], ref, sb, (short)sections);

		diff_eq_int(what, ya, yb, i);
		if (ya != 0)
			nonzero++;
	}
	for (i = 0; i < 2 * sections; i++)
		diff_eq_int("iir state", sa[i], sb[i], i);

	diff_eq_int("%s: the filter produced a signal", nonzero > BLOCK / 2, 1,
		    0);
	if (nonzero > BLOCK / 2)
		saw_signal++;
}

int
main(void)
{
	int i;
	int nonzero;

	diff_begin("V23filt.c: the tables, word for word");
	compare("_V23_MRF_FILT[%ld]", _V23_MRF_FILT, ref__V23_MRF_FILT, 48);
	compare("_V23RX_ANSWER_INTRP[%ld]", _V23RX_ANSWER_INTRP,
		ref__V23RX_ANSWER_INTRP, 15);
	compare("_V23RX_IIR_LPF[%ld]", _V23RX_IIR_LPF, ref__V23RX_IIR_LPF, 15);
	compare("V23_IIR_FILT[%ld]", V23_IIR_FILT, ref_V23_IIR_FILT, 20);

	/*
	 * The structure the header claims, checked rather than asserted.  Each
	 * of these is a statement the coefficient ORDER was deduced from, so a
	 * failure here means the documentation is wrong even though the bytes
	 * are right -- which is the more expensive kind of error, because
	 * everything written against the wrong reading inherits it.
	 */
	for (i = 0; i < 24; i++)
		diff_eq_int("_V23_MRF_FILT is symmetric about tap 24",
			    _V23_MRF_FILT[i], _V23_MRF_FILT[47 - i], i);

	for (i = 0; i < 4; i++)
		diff_eq_int("V23_IIR_FILT section %ld has b0 == b2",
			    V23_IIR_FILT[5 * i], V23_IIR_FILT[5 * i + 1], i);

	/*
	 * The interpolator's centre tap dominates -- that is what makes it a
	 * fractional delay rather than a shaping filter.  Checked as "no other
	 * tap comes within a factor of seven", which the alternating tail
	 * satisfies with room to spare and a mistyped table would not.
	 */
	for (i = 0; i < 15; i++) {
		int t = _V23RX_ANSWER_INTRP[i];

		if (i == 7)
			continue;
		if (t < 0)
			t = -t;
		diff_eq_int("_V23RX_ANSWER_INTRP tap %ld is well below the "
			    "centre", t * 7 < 16011, 1, i);
	}

	/* Nothing here is a table of zeroes. */
	for (nonzero = 0, i = 0; i < 48; i++)
		nonzero += _V23_MRF_FILT[i] != 0;
	diff_eq_int("_V23_MRF_FILT is not empty", nonzero, 48, 0);
	for (nonzero = 0, i = 0; i < 20; i++)
		nonzero += V23_IIR_FILT[i] != 0;
	diff_eq_int("V23_IIR_FILT is not empty", nonzero, 20, 0);

	/*
	 * And equivalent where it counts: through the engines the object
	 * actually pairs each table with.  v23FP_rx_progress runs the channel
	 * filter through FPM_iir_filt_II; FPM_FSD reaches the discriminator
	 * lowpass through FPM_iir_filt.
	 */
	filter_ii("V23_IIR_FILT through FPM_iir_filt_II[%ld]", V23_IIR_FILT,
		  ref_V23_IIR_FILT, 4);
	filter_i("_V23RX_IIR_LPF through FPM_iir_filt[%ld]", _V23RX_IIR_LPF,
		 ref__V23RX_IIR_LPF, 3);

	diff_eq_int("both filters were exercised on a real signal", saw_signal,
		    2, 0);

	return diff_end();
}

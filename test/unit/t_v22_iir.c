/*
 * t_v22_iir.c -- differential test of V.22's own IIR and its mixer.
 *
 * Three things here are easy to get wrong and invisible in a short run:
 *
 *   - the denominator is read from a[1], not a[0].  Reading it from a[0]
 *     gives a stable filter with the wrong response, and the real
 *     coefficients happen to make the difference small at low amplitude.
 *   - each product is shifted down by 14 separately.  Accumulating first and
 *     shifting once agrees on most samples and disagrees on some, so the
 *     stream has to be long.
 *   - the mixer multiplies the 16-bit TRUNCATION of the accumulator, not the
 *     accumulator.  That only shows once something wraps, which ordinary
 *     signal levels never do -- hence the deliberately overdriven cases and
 *     the non-vacuity guard at the end.
 *
 * State is compared after every block, and the input to the next block is
 * fresh, so the histories have to carry correctly across calls.
 */

#include <string.h>

#include "harness.h"
#include "dsplib/v22_iir.h"

extern void ref_V22IIRFilterInit(short *state, const short *b, const short *a);
extern void ref_V22_iir_filt_demod(short *samples, const short *b,
				   const short *a, short *xhist, short *yhist,
				   const short *mix);
extern short ref_IIR_b_coeff[];
extern short ref_IIR_a_coeff[];

/* Set by the identity-filter run below; see the guard in main(). */
static int mixer_overflowed;
static int filter_wrapped;

/*
 * A marker no coefficient or history value can be confused with.  Init must
 * overwrite every one of the sixteen words, so any survivor is a field the
 * reconstruction failed to write.
 */
#define MARK 0x5ead

static int
run_init(const char *label, const short *b, const short *a)
{
	short sa[V22_IIR_STATE_WORDS], sb[V22_IIR_STATE_WORDS];
	int rc, i;

	for (i = 0; i < V22_IIR_STATE_WORDS; i++)
		sa[i] = sb[i] = (short)MARK;

	diff_begin(label);
	ref_V22IIRFilterInit(sa, b, a);
	V22IIRFilterInit(sb, b, a);

	for (i = 0; i < V22_IIR_STATE_WORDS; i++) {
		diff_eq_int("state[%ld]", sb[i], sa[i], i);
		diff_eq_int("state[%ld] was written", sb[i] != (short)MARK, 1, i);
	}
	rc = diff_end();
	return rc;
}

/*
 * `blocks` passes of V22_IIR_BLOCK samples.  `amplitude` scales the input,
 * `mixamp` the carrier; both sides see identical arrays.
 */
static int
run_filt(const char *label, const short *b, const short *a, int blocks,
	 int amplitude, int mixamp, int identity)
{
	short sa[V22_IIR_STATE_WORDS], sb[V22_IIR_STATE_WORDS];
	short ba[V22_IIR_BLOCK], bb[V22_IIR_BLOCK];
	short in[V22_IIR_BLOCK], mix[V22_IIR_BLOCK];
	unsigned lfsr = 0x7E22u;
	int rc, n, i;

	ref_V22IIRFilterInit(sa, b, a);
	V22IIRFilterInit(sb, b, a);

	diff_begin(label);
	for (n = 0; n < blocks; n++) {
		for (i = 0; i < V22_IIR_BLOCK; i++) {
			lfsr = (lfsr >> 1) ^ (-(int)(lfsr & 1u) & 0xB400u);
			in[i] = (short)(((int)(lfsr & 0xffff) - 0x8000)
					/ (0x8000 / amplitude));
			lfsr = (lfsr >> 1) ^ (-(int)(lfsr & 1u) & 0xB400u);
			mix[i] = (short)(((int)(lfsr & 0xffff) - 0x8000)
					 / (0x8000 / mixamp));
			ba[i] = bb[i] = in[i];
		}

		ref_V22_iir_filt_demod(ba, sa + V22_IIR_OFF_B,
				       sa + V22_IIR_OFF_A, sa + V22_IIR_OFF_X,
				       sa + V22_IIR_OFF_Y, mix);
		V22_iir_filt_demod(bb, sb + V22_IIR_OFF_B, sb + V22_IIR_OFF_A,
				   sb + V22_IIR_OFF_X, sb + V22_IIR_OFF_Y, mix);

		for (i = 0; i < V22_IIR_BLOCK; i++)
			diff_eq_int("sample %ld: output", bb[i], ba[i],
				    n * V22_IIR_BLOCK + i);
		for (i = 0; i < V22_IIR_STATE_WORDS; i++)
			diff_eq_int("block state[%ld]", sb[i], sa[i], i);

		/*
		 * Non-vacuity, and exact rather than a heuristic.  With
		 * b = { 16384, 0, 0, 0 } and a = { 16384, 0, 0, 0 } the
		 * filter is the identity -- acc = (16384 * x) >> 14 = x -- so
		 * the product the mixer forms is known here without modelling
		 * anything the test is checking.  Count the ones that leave
		 * 16 bits: those are the samples where "truncate" and
		 * "saturate" would disagree.
		 */
		if (identity) {
			for (i = 0; i < V22_IIR_BLOCK; i++) {
				int p = ((int)in[i] * mix[i]) >> 12;

				if (p > 32767 || p < -32768)
					mixer_overflowed++;
			}
		}

		/*
		 * A y-history word at the top of the range after a run is the
		 * signature of an accumulator that left 16 bits; this filter
		 * has no clamp to stick at.
		 */
		for (i = 0; i < V22_IIR_Y_USED; i++)
			if (sa[V22_IIR_OFF_Y + i] > 30000
			    || sa[V22_IIR_OFF_Y + i] < -30000)
				filter_wrapped++;
	}
	rc = diff_end();
	return rc;
}

int
main(void)
{
	static const short identity_b[4] = { 16384, 0, 0, 0 };
	static const short identity_a[4] = { 16384, 0, 0, 0 };
	/*
	 * a[1] = -16384 makes the recursion y[n] = x[n] + y[n-1], an
	 * integrator: sustained input walks it out of 16 bits within a few
	 * hundred samples.  Nothing else reaches the wrap.
	 */
	static const short unstable_a[4] = { 16384, -16384, 0, 0 };
	/*
	 * Distinct in every word, so a transposed copy in the init shows up
	 * rather than cancelling.
	 */
	static const short marker_b[4] = { 1, 2, 3, 4 };
	static const short marker_a[4] = { -5, -6, -7, -8 };
	int rc = 0;
	int i;

	/* --- the tables ------------------------------------------------ */

	diff_begin("v22 iir coefficient tables");
	for (i = 0; i < V22_IIR_B_TAPS; i++)
		diff_eq_int("IIR_b_coeff[%ld]", IIR_b_coeff[i],
			    ref_IIR_b_coeff[i], i);
	for (i = 0; i < V22_IIR_A_TAPS; i++)
		diff_eq_int("IIR_a_coeff[%ld]", IIR_a_coeff[i],
			    ref_IIR_a_coeff[i], i);
	rc |= diff_end();

	/* --- V22IIRFilterInit ------------------------------------------ */

	rc |= run_init("v22 iir init, real coefficients",
		       ref_IIR_b_coeff, ref_IIR_a_coeff);
	rc |= run_init("v22 iir init, marker coefficients",
		       marker_b, marker_a);

	/* --- V22_iir_filt_demod ---------------------------------------- */

	/* The real filter, at ordinary receive levels. */
	rc |= run_filt("v22 iir real coefficients", ref_IIR_b_coeff,
		       ref_IIR_a_coeff, 8, 8000, 8000, 0);

	/* Full scale, where the separate >> 14 truncations diverge most. */
	rc |= run_filt("v22 iir real coefficients full scale",
		       ref_IIR_b_coeff, ref_IIR_a_coeff, 8, 32767, 32767, 0);

	/* A quiet carrier: the mixer's >> 12 then dominates. */
	rc |= run_filt("v22 iir quiet carrier", ref_IIR_b_coeff,
		       ref_IIR_a_coeff, 4, 32767, 64, 0);

	/*
	 * The identity filter.  Its only purpose is to make the mixer's
	 * arithmetic predictable, so the guard below can be exact.
	 */
	rc |= run_filt("v22 iir identity, mixer only", identity_b, identity_a,
		       8, 32767, 32767, 1);

	/* The integrator, to drive the accumulator out of sixteen bits. */
	rc |= run_filt("v22 iir integrator", identity_b, unstable_a, 8, 32767,
		       4096, 0);

	/* --- the guards ------------------------------------------------ */

	/*
	 * Both of these would go quiet if the drive levels above were ever
	 * softened, and with them the only coverage of the two places where
	 * this filter differs from a saturating one.  Finding F134's argument:
	 * a detector that has never been shown to fire is not a detector.
	 */
	diff_begin("v22 iir overflow paths reached");
	diff_eq_int("mixer product left 16 bits (%ld)", mixer_overflowed > 0,
		    1, 0);
	diff_eq_int("filter accumulator wrapped (%ld)", filter_wrapped > 0, 1,
		    0);
	rc |= diff_end();

	return rc;
}

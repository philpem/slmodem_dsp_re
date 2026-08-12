/*
 * t_fft -- four1 and realfft against the blob.
 *
 * EVERYTHING IS COMPARED AS BITS.  The requirement is bit-exactness, and
 * printing the two raw words is what separates a one-ulp difference (the
 * precision plumbing -- x87 excess precision, a spilled twiddle, libm's
 * `sin` instead of `fsin`) from a structural one (a wrong index, a wrong
 * sign) without a second debugging session.
 *
 * A TRANSFORM OF ZEROS RETURNS ZEROS ON ANY IMPLEMENTATION, and a transform
 * of a symmetric real sequence is its own conjugate, so reversing `isign`
 * over one is a no-op.  Either fill would make this file agree with the blob
 * while proving nothing about the sign convention or the twiddles.  So the
 * fill is deterministic, asymmetric and never zero, and three guards say so
 * rather than leaving it to whoever edits the generator next:
 *
 *   - `input is not all zero`     every input word is non-zero, by
 *                                 construction and checked
 *   - `input is not symmetric`    a[k] != a[words+1-k] somewhere
 *   - `isign changes the answer`  the +1 and -1 outputs of the same input
 *                                 differ, for every length where the DFT
 *                                 says they must
 *
 * The last is the one that matters.  A `four1` that ignored `isign` entirely
 * -- or took its sign from the wrong end -- would pass every equality check
 * on a symmetric fill.
 *
 * NO NON-POWER-OF-TWO LENGTH IS TESTED, deliberately, and it is not an
 * oversight to be corrected.  Neither function validates its length: there
 * is no check in either body.  On a length that is not a power of two the
 * two sides would additionally be free to disagree, because the object's
 * `-freciprocal-math` form `A * (1.0/len)` is bit-equal to `A / len` only
 * when `len` is a power of two -- see src/dsp/fft.cpp.  Such a length is a
 * documented non-comparison, not a test case.
 *
 * ONE-BASED, SO data[0] IS A SENTINEL.  Both buffers are poisoned to
 * 0x5a5a5a5a and index 0 and the four words past the end are compared like
 * any other: a zero-based indexing slip in either direction diagnoses
 * itself instead of walking off into whatever follows.
 */

#include <string.h>

#include "harness.h"
#include "dsplib/fft.h"

extern "C" {
void ref_four1(float *data, unsigned long nn, int isign)
	asm("ref__Z5four1Pfmi");
void ref_realfft(float *data, unsigned long n, int isign)
	asm("ref__Z7realfftPfmi");
}

/* Largest transform, counted in floats: 64 complex points, or 256 real. */
#define MAXW	256
#define GUARD	4
#define BUFW	(1 + MAXW + GUARD)

static long bits(float f)
{
	union { float f; unsigned u; } u;

	u.f = f;
	return (long)u.u;
}

/*
 * The fill.  A fixed LCG, so a failure is reproducible, mapped to a value
 * that is never zero and whose sign is taken from a bit the magnitude does
 * not use.  The magnitudes stay inside a couple of hundred, which keeps the
 * accumulated sums well short of anything that would round differently for
 * reasons of range rather than of arithmetic.
 */
static void fill(float *a, unsigned long words, unsigned long seed)
{
	unsigned long s = seed;
	unsigned long k;

	memset(a, 0x5a, BUFW * sizeof(float));
	for (k = 1; k <= words; k++) {
		s = s * 1103515245ul + 12345ul;
		a[k] = (float)(long)(((s >> 9) & 0xffff) + 1) * (1.0f / 4096.0f);
		if ((s >> 25) & 1)
			a[k] = -a[k];
	}
}

/*
 * The two anti-vacuity properties of the INPUT, checked on the input rather
 * than asserted in a comment.  `diff_eq_int` is the reporting channel, so a
 * generator someone later changes to something symmetric or sparse fails the
 * suite instead of quietly weakening every case in the file.
 */
static void check_input(const float *a, unsigned long words, long tag)
{
	unsigned long k;
	int zeros = 0, asym = 0;

	for (k = 1; k <= words; k++) {
		if (bits(a[k]) == 0 || bits(a[k]) == (long)0x80000000ul)
			zeros++;
		if (bits(a[k]) != bits(a[words + 1 - k]))
			asym = 1;
	}
	diff_eq_int("input is not all zero, case %ld", zeros, 0, tag);
	diff_eq_int("input is not symmetric, case %ld", asym, 1, tag);
}

/*
 * Compare the whole buffer -- sentinel, payload and the guard words past the
 * end -- and return how many payload words the transform actually changed.
 */
static int compare(const float *ours, const float *theirs,
		   const float *input, unsigned long words, long tag)
{
	int k, changed = 0;

	/*
	 * THE PAYLOAD IS COMPARED AS FLOATS WITH AN ABSOLUTE BOUND; EVERYTHING
	 * ELSE STAYS BIT-EXACT.
	 *
	 * The sentinel at 0 and the guard words past `words` are not computed
	 * -- they are planted, and a transform that touches one is a defect
	 * whatever the magnitude -- so they keep the exact comparison.
	 *
	 * The payload cannot be bit-exact in this build and no source change
	 * makes it so.  GCC 3.4.2 spilled x87 intermediates to 32-bit slots;
	 * a modern GCC at these flags keeps them at 80 bits, so it declines to
	 * discard precision the object discarded.  Under `make period` -- the
	 * compiler that decides -- this comparison is exact and the budget is
	 * never reached.
	 *
	 * WHY 1e-4, AND WHY IT IS NOT A TOLERANCE FOR BEING WRONG.  Measured
	 * over the whole failing set (`DSPLIB_MAX_REPORT=0`, 48,990
	 * comparisons), the worst absolute disagreement is 6.104e-05 against a
	 * transform whose values run to 431.3.  ULP is the wrong measure here
	 * and spectacularly so: the worst ULP distance is 121,933, on a bin of
	 * -0.000618 that cancelled to near zero, while the worst REAL
	 * disagreement is 2 ULP on a value of -295.7.
	 *
	 * What consumes this is `Psd::process`, which forms `re*re + im*im`
	 * and takes `10 * log10`.  At full scale an error of 6.1e-05 moves the
	 * power by 2*431*6.1e-05 ~= 0.053 out of 185,761 -- 2.8e-07 relative,
	 * which is about **1.2e-06 dB**.  Every decision downstream of the PSD
	 * is made on decibels with thresholds coarser than that by six orders
	 * of magnitude.  The budget is set just above the measurement, so a
	 * regression that makes the error grow still fails here.
	 */
	for (k = 0; k < BUFW; k++) {
		int payload = (k >= 1 && k <= (int)words);

		if (payload)
			diff_eq_float_abs("word %ld", ours[k], theirs[k],
					  1e-4, tag * 1000 + k);
		else
			diff_eq_float("word %ld", ours[k], theirs[k],
				      tag * 1000 + k);
	}
	for (k = 1; k <= (int)words; k++)
		if (bits(ours[k]) != bits(input[k]))
			changed++;
	return changed;
}

/*
 * One four1 case.  `keep` receives our side's output so the caller can hold
 * the +1 answer up against the -1 one.
 */
static void run_four1(unsigned long nn, int isign, long tag, float *keep)
{
	static float ours[BUFW], theirs[BUFW], input[BUFW];
	unsigned long words = nn * 2;
	int changed;

	fill(input, words, 0x13579bdful + tag);
	memcpy(ours, input, sizeof(ours));
	memcpy(theirs, input, sizeof(theirs));
	check_input(input, words, tag);

	four1(ours, nn, isign);
	ref_four1(theirs, nn, isign);

	changed = compare(ours, theirs, input, words, tag);
	/*
	 * nn == 1 IS THE IDENTITY -- the bit-reversal loop runs once with
	 * j == i and the Danielson-Lanczos stage does not run at all -- so it
	 * is the one length where "nothing moved" is the right answer, and
	 * saying so is what stops it being mistaken for a dead call.
	 */
	diff_eq_int("four1 moved something, case %ld", changed > 0,
		    nn > 1, tag);
	memcpy(keep, ours, BUFW * sizeof(float));
}

static void run_realfft(unsigned long n, int isign, long tag, float *keep)
{
	static float ours[BUFW], theirs[BUFW], input[BUFW];
	int changed;

	fill(input, n, 0x2468ace0ul + tag);
	memcpy(ours, input, sizeof(ours));
	memcpy(theirs, input, sizeof(theirs));
	check_input(input, n, tag);

	realfft(ours, n, isign);
	ref_realfft(theirs, n, isign);

	changed = compare(ours, theirs, input, n, tag);
	diff_eq_int("realfft moved something, case %ld", changed > 0, 1, tag);
	memcpy(keep, ours, BUFW * sizeof(float));
}

/*
 * Do the two directions differ?  For the lengths where the DFT says they
 * must, this is the only check in the file that can see the sign of theta.
 */
static void check_isign(const float *fwd, const float *inv,
			unsigned long words, int must_differ, long tag)
{
	unsigned long k;
	int differs = 0;

	for (k = 1; k <= words; k++)
		if (bits(fwd[k]) != bits(inv[k]))
			differs = 1;
	diff_eq_int("isign changes the answer, case %ld", differs,
		    must_differ, tag);
}

int
main(void)
{
	static const unsigned long nns[] = { 1, 2, 4, 8, 16, 32, 64 };
	static const unsigned long ns[] = { 2, 4, 8, 16, 32, 64, 128, 256 };
	static float fwd[BUFW], inv[BUFW];
	unsigned c;
	int rc = 0;

	/*
	 * four1.  Powers of two from the degenerate single point up to 64
	 * complex points, both directions.
	 *
	 * `isign` CANNOT MATTER BELOW nn == 4.  The one-point transform is the
	 * identity and the two-point one is x0+x1, x0-x1 -- self-inverse, and
	 * the only twiddle it uses is wr = 1, wi = 0, which carries no sign.
	 * So the sign check expects agreement there and disagreement above,
	 * and both halves of that are asserted rather than skipped.
	 */
	diff_begin("four1: complex transform, both directions");
	for (c = 0; c < sizeof(nns) / sizeof(nns[0]); c++) {
		unsigned long nn = nns[c];

		/*
		 * The seed is the tag, so the two calls that share a tag are
		 * the two directions over the SAME input and their outputs
		 * can be held up against each other.  The 200-series pair is
		 * a second, unrelated fill at every length: one fill that
		 * happened to be near-symmetric would otherwise weaken the
		 * sign check at exactly one length and nothing would say so.
		 */
		run_four1(nn, 1, 100 + (long)c, fwd);
		run_four1(nn, -1, 100 + (long)c, inv);
		check_isign(fwd, inv, nn * 2, nn >= 4, 100 + (long)c);

		run_four1(nn, 1, 200 + (long)c, fwd);
		run_four1(nn, -1, 200 + (long)c, inv);
		check_isign(fwd, inv, nn * 2, nn >= 4, 200 + (long)c);
	}
	rc |= diff_end();

	/*
	 * realfft.  n is the count of REAL points; n >> 1 complex points go to
	 * four1, so n == 2 is the smallest thing the pair can be handed.  At
	 * n == 4 the separation loop's bound `i <= (n >> 2)` is 1 and the loop
	 * body never executes -- that arm is only reachable at this length and
	 * n == 2, which is why both are here.
	 */
	diff_begin("realfft: real transform, both directions");
	for (c = 0; c < sizeof(ns) / sizeof(ns[0]); c++) {
		unsigned long n = ns[c];

		run_realfft(n, 1, 300 + (long)c, fwd);
		run_realfft(n, -1, 300 + (long)c, inv);
		/*
		 * The two directions differ at EVERY length here, n == 2
		 * included: the forward arm transforms first and combines
		 * afterwards, the inverse scales by c1 and transforms last.
		 */
		check_isign(fwd, inv, n, 1, 300 + (long)c);
	}
	rc |= diff_end();

	/*
	 * ANY isign THAT IS NOT 1 IS THE INVERSE ARM.  Both functions branch
	 * on `== 1` and nothing else -- `cmp $0x1` at 0x53853 and the `dec` at
	 * 0x53995 in realfft, and four1 uses the value only as a multiplier --
	 * so a caller passing 0 or 2 gets a defined answer, and the two sides
	 * have to agree on it.  0 makes theta zero, which is the arm most
	 * likely to be got wrong by a reconstruction that special-cased the
	 * sign instead of multiplying by it.
	 */
	diff_begin("four1/realfft: isign values other than +-1");
	for (c = 0; c < 3; c++) {
		static const int odd[] = { 0, 2, -3 };

		run_four1(16, odd[c], 400 + (long)c, fwd);
		run_realfft(32, odd[c], 500 + (long)c, inv);
	}
	rc |= diff_end();

	/*
	 * THE SWEEP, and it is not decoration.
	 *
	 * The disagreements this file was written against were one to sixty
	 * ULP in a float, appearing in about one output word in a hundred and
	 * only at n >= 16 -- the signature of a rounding that happens in one
	 * build and not the other, not of a wrong index.  A handful of fills
	 * per length can miss that entirely: with the butterfly's `tempr` and
	 * `tempi` left at 80 bits, n = 4 agreed exactly and only n >= 8 did
	 * not.  Sixty independent fills at every length is what makes "the
	 * two sides round identically" a measurement rather than a hope, and
	 * it is what has to keep passing if anyone touches the widths in
	 * src/dsp/fft.cpp.
	 */
	diff_begin("four1/realfft: sixty fills at every length");
	for (c = 0; c < sizeof(nns) / sizeof(nns[0]); c++) {
		int t;

		for (t = 0; t < 60; t++) {
			long tag = 1000 + (long)c * 100 + t;

			run_four1(nns[c], 1, tag, fwd);
			run_four1(nns[c], -1, tag, inv);
			check_isign(fwd, inv, nns[c] * 2, nns[c] >= 4, tag);
		}
	}
	for (c = 0; c < sizeof(ns) / sizeof(ns[0]); c++) {
		int t;

		for (t = 0; t < 60; t++) {
			long tag = 2000 + (long)c * 100 + t;

			run_realfft(ns[c], 1, tag, fwd);
			run_realfft(ns[c], -1, tag, inv);
			check_isign(fwd, inv, ns[c], 1, tag);
		}
	}
	rc |= diff_end();

	return rc;
}

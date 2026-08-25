/*
 * t_dspmath -- the seven float statistics templates against the blob.
 *
 * These are weak symbols, one per `.gnu.linkonce.t.*` section, and until
 * finding F243 they were invisible to `make coverage` on both sides.  They
 * have `ref_` aliases like anything else, so nothing about testing them is
 * special -- only the `extern "C"` and the asm label, because the alias is
 * `ref_` prepended to the raw mangled string and a plain C++ declaration
 * would mangle it a second time (finding F225).
 *
 * EVERYTHING IS COMPARED AS BITS.  These return floats through st(0), and a
 * NaN compares unequal to itself, so `==` would report a difference that is
 * not one and miss one that is.  The union below is the whole of the trick.
 */

#include <string.h>

#include "harness.h"
#include "dsplib/DspMath.h"

extern "C" {
float ref_sum(float *x, unsigned n)     asm("ref__Z3sumIfET_PS0_j");
float ref_mean(float *x, unsigned n)    asm("ref__Z4meanIfET_PS0_j");
float ref_sqrSum(float *x, unsigned n)  asm("ref__Z6sqrSumIfET_PS0_j");
float ref_Var(float *x, unsigned n)     asm("ref__Z3VarIfET_PS0_j");
float ref_Std(float *x, unsigned n)     asm("ref__Z3StdIfET_PS0_j");
float ref_sinc(float x)                 asm("ref__Z4sincIfET_S0_");
void ref_boxcar(float *w, unsigned n)   asm("ref__Z6boxcarIfEvPT_j");
void ref_hanning(float *w, unsigned n)  asm("ref__Z7hanningIfEvPT_j");
void ref_hamming(float *w, unsigned n)  asm("ref__Z7hammingIfEvPT_j");
void ref_blackman(float *w, unsigned n) asm("ref__Z8blackmanIfEvPT_j");
void ref_designWindow(WindowType t, float *w, unsigned n)
	asm("ref__Z12designWindowIfEv10WindowTypePT_j");
}

static long bits(float f)
{
	union { float f; unsigned u; } u;

	u.f = f;
	return (long)u.u;
}

/*
 * The inputs.  Deliberately not all well-behaved: `Var` subtracts two nearly
 * equal numbers, so a constant array is where catastrophic cancellation
 * shows, and a large-mean array is where it shows worst.
 */
static void fill(float *x, unsigned n, int shape)
{
	unsigned i;

	for (i = 0; i < n; i++) {
		switch (shape) {
		case 0: x[i] = (float)i; break;
		case 1: x[i] = 1.0f; break;			/* Var == 0   */
		case 2: x[i] = 1000000.0f + (float)i; break;	/* cancels    */
		case 3: x[i] = (i & 1) ? -1.0f : 1.0f; break;
		case 4: x[i] = (float)i * 1e-7f; break;		/* tiny       */
		case 5: x[i] = (float)((i * 37) % 101) - 50.0f; break;
		case 6: x[i] = 0.0f; break;			/* all zero   */
		default: x[i] = 1e20f; break;			/* overflows  */
		}
	}
}

int
main(void)
{
	static float x[512], wa[512], wb[512];
	int rc = 0;
	unsigned n, shape, i;

	diff_begin("dspmath: sum, mean, sqrSum, Var, Std");
	for (shape = 0; shape < 8; shape++)
		for (n = 0; n <= 64; n++) {
			long tag = (long)shape * 1000 + n;

			fill(x, n ? n : 1, (int)shape);

			diff_eq_int("sum", bits(sum(x, n)),
				    bits(ref_sum(x, n)), tag);
			diff_eq_int("mean", bits(mean(x, n)),
				    bits(ref_mean(x, n)), tag);
			diff_eq_int("sqrSum", bits(sqrSum(x, n)),
				    bits(ref_sqrSum(x, n)), tag);
			diff_eq_int("Var", bits(Var(x, n)),
				    bits(ref_Var(x, n)), tag);
			diff_eq_int("Std", bits(Std(x, n)),
				    bits(ref_Std(x, n)), tag);
		}
	rc |= diff_end();

	/*
	 * n == 0 is driven above and is worth saying out loud: every one of
	 * the five divides by it, so all but `sum` return a NaN or an
	 * infinity.  Compared as bits, those compare equal; compared as
	 * floats a NaN would fail against itself.
	 */
	diff_begin("dspmath: sinc");
	{
		/* Zero, the exact singularity, and either side of it. */
		static const float pts[] = {
			0.0f, -0.0f, 1e-30f, -1e-30f, 1e-7f, 0.5f, -0.5f,
			1.0f, -1.0f, 1.5f, 2.0f, 3.0f, 10.0f, 100.0f,
			-100.0f, 0.25f, 0.125f, 1e6f, -1e6f
		};

		for (i = 0; i < sizeof(pts) / sizeof(pts[0]); i++)
			diff_eq_int("sinc", bits(sinc(pts[i])),
				    bits(ref_sinc(pts[i])), (long)i);

		/* And a sweep, so it is not only the chosen points. */
		for (i = 0; i < 4000; i++) {
			float v = (float)((int)i - 2000) / 97.0f;

			diff_eq_int("sinc sweep", bits(sinc(v)),
				    bits(ref_sinc(v)), (long)i);
		}
	}
	rc |= diff_end();

	diff_begin("dspmath: boxcar");
	for (n = 0; n <= 64; n++) {
		memset(wa, 0x5a, sizeof(wa));
		memset(wb, 0x5a, sizeof(wb));
		boxcar(wa, n);
		ref_boxcar(wb, n);
		/*
		 * The whole buffer, not the first `n`: a version that wrote
		 * one element too many would pass a comparison that stopped
		 * at `n`.
		 */
		for (i = 0; i < 512; i++)
			diff_eq_int("boxcar", bits(wa[i]), bits(wb[i]),
				    (long)n * 1000 + i);
	}
	rc |= diff_end();

	/*
	 * THE THREE COSINE WINDOWS, over the whole buffer so that a version
	 * writing one element too many is caught rather than passing.
	 *
	 * n == 1 is deliberately included and is not an edge case to be
	 * skipped: `hamming` and `blackman` both divide by n-1, so it is a
	 * division by zero and both write the x87 indefinite, 0xffc00000.
	 * Compared as bits, which is the only way a NaN compares equal to
	 * itself.
	 */
	diff_begin("dspmath: the cosine windows");
	for (n = 0; n <= 300; n++) {
		memset(wa, 0x5a, sizeof(wa));
		memset(wb, 0x5a, sizeof(wb));
		hanning(wa, n);
		ref_hanning(wb, n);
		for (i = 0; i < 512; i++)
			diff_eq_int("hanning", bits(wa[i]), bits(wb[i]),
				    (long)n * 1000 + i);

		memset(wa, 0x5a, sizeof(wa));
		memset(wb, 0x5a, sizeof(wb));
		hamming(wa, n);
		ref_hamming(wb, n);
		for (i = 0; i < 512; i++)
			diff_eq_int("hamming", bits(wa[i]), bits(wb[i]),
				    (long)n * 1000 + i);

		memset(wa, 0x5a, sizeof(wa));
		memset(wb, 0x5a, sizeof(wb));
		blackman(wa, n);
		ref_blackman(wb, n);
		for (i = 0; i < 512; i++)
			diff_eq_int("blackman", bits(wa[i]), bits(wb[i]),
				    (long)n * 1000 + i);
	}
	rc |= diff_end();

	/*
	 * The selector, over every arm AND over values outside the enum: the
	 * object sends 0, anything negative and anything above 3 to `boxcar`,
	 * by two different routes that are the same behaviour.
	 */
	diff_begin("dspmath: designWindow selects the right one");
	{
		static const int types[] = { 0, 1, 2, 3, 4, 99, -1, -7 };
		unsigned k;

		for (k = 0; k < sizeof(types) / sizeof(types[0]); k++)
			for (n = 0; n <= 40; n++) {
				memset(wa, 0x5a, sizeof(wa));
				memset(wb, 0x5a, sizeof(wb));
				designWindow((WindowType)types[k], wa, n);
				ref_designWindow((WindowType)types[k], wb, n);
				for (i = 0; i < 512; i++)
					diff_eq_int("designWindow",
						    bits(wa[i]), bits(wb[i]),
						    ((long)k * 100 + n) * 1000
						    + i);
			}
	}
	rc |= diff_end();

	return rc;
}

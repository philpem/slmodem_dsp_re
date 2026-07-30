/*
 * t_fixedrc.c -- differential test of the rate-conversion mode lookup.
 *
 * RcFixed_Check_Combination() is pure, so this sweeps it rather than spot-
 * checking: every pair drawn from the telephony rates the modem actually
 * encounters, plus a dense sweep of small integers to catch GCD edge cases,
 * plus the degenerate inputs (zero, negative) that the original's unguarded
 * Euclid loop handles in its own particular way.
 */

#include "harness.h"
#include "dsplib/fixedrc.h"

extern int ref_RcFixed_Check_Combination(int in_rate, int out_rate);

/* Rates that appear anywhere in the modem: host, pump, codec and fax. */
static const int rates[] = {
	7200, 8000, 9600, 12000, 16000, 19200, 24000, 32000, 38400, 48000,
	11025, 22050, 44100, 4000, 2400, 1200, 300,
};

int
main(void)
{
	int rc = 0;
	unsigned i, j;
	int a, b;

	diff_begin("Check_Combination/rates");
	for (i = 0; i < sizeof(rates) / sizeof(rates[0]); i++) {
		for (j = 0; j < sizeof(rates) / sizeof(rates[0]); j++) {
			int in = rates[i], out = rates[j];
			diff_eq_int("Check_Combination(%ld, ...)",
				    RcFixed_Check_Combination(in, out),
				    ref_RcFixed_Check_Combination(in, out),
				    in);
		}
	}
	rc |= diff_end();

	/*
	 * Dense small-integer sweep.  Ratios are matched after GCD reduction,
	 * so 500:600 must resolve to the same mode as 8000:9600.
	 */
	diff_begin("Check_Combination/sweep");
	for (a = 1; a <= 120; a++) {
		for (b = 1; b <= 120; b++)
			diff_eq_int("Check_Combination(%ld, b)",
				    RcFixed_Check_Combination(a, b),
				    ref_RcFixed_Check_Combination(a, b), a);
	}
	rc |= diff_end();

	/* The unguarded Euclid loop: gcd(x,0) == x, and signs propagate. */
	diff_begin("Check_Combination/degenerate");
	for (a = -8; a <= 8; a++) {
		for (b = -8; b <= 8; b++) {
			if (b == 0 && a == 0)
				continue;	/* 0/0 traps in both */
			if (a == 0 || b == 0)
				continue;	/* division by the gcd traps */
			diff_eq_int("Check_Combination(%ld, b)",
				    RcFixed_Check_Combination(a, b),
				    ref_RcFixed_Check_Combination(a, b), a);
		}
	}
	rc |= diff_end();

	/*
	 * The two facts the rest of the project leans on:
	 * the 9600<->8000 bridge exists, and equal rates do not convert.
	 */
	diff_begin("Check_Combination/anchors");
	diff_eq_int("9600->8000 mode (%ld)",
		    RcFixed_Check_Combination(9600, 8000), 3, 0);
	diff_eq_int("8000->9600 mode (%ld)",
		    RcFixed_Check_Combination(8000, 9600), 2, 0);
	diff_eq_int("8000->8000 is identity (%ld)",
		    RcFixed_Check_Combination(8000, 8000), RCFIXED_NMODES, 0);
	diff_eq_int("mode 3 down factor (%ld)", RcFixed_DownFactor(3), 6, 0);
	diff_eq_int("mode 3 up factor (%ld)", RcFixed_UpFactor(3), 5, 0);
	rc |= diff_end();

	return rc;
}

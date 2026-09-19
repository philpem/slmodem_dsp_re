/*
 * t_float_tol.c -- the negative control for the modern tier's float tolerance.
 *
 * THE PROBLEM THIS SOLVES.  The modern tier (GCC 14, x86-64) is a portability
 * check, not the reconstruction authority; `make period` (GCC 3.4.2-r2) is
 * byte-exact against the blob and has no allow-list.  A rounding-level x87
 * difference on the modern build must not read as a reconstruction failure --
 * but a tolerance is a detector, and a detector that cannot fail is dead
 * (findings F134, F2400, F2401).  This binary proves BOTH directions, and it
 * is ADAPTIVE so it is meaningful in either build:
 *
 *   with -DHARNESS_FLOAT_TOL   a 1-ULP pair must PASS (and be counted as
 *                              tolerance-only), a beyond-eps pair must FAIL,
 *                              and a relative difference near zero must FAIL
 *                              (the criterion is relative, not absolute);
 *
 *   without the define         both the 1-ULP and the beyond-eps pair must
 *                              FAIL -- the comparison is bit-for-bit, exactly
 *                              as the period build has always been.
 *
 * WHICH ARM IT IS IS ASKED OF THE LINKED HARNESS, not of this file's own
 * preprocessor: the tolerance lives in harness.o and a translation unit built
 * without the define would otherwise disagree with the object it calls.
 *
 * It links only the comparison support: no reconstruction, no blob, no
 * DSPLIB_REPRODUCE_BUGS.  Expected failures are silenced by setting
 * diff_max_report to 0, but diff_failures still counts them, so the assertions
 * below are the check and the final line is the denominator.
 */

#include <stdio.h>
#include <string.h>

#include "harness.h"

static float
from_bits(unsigned u)
{
	float f;

	memcpy(&f, &u, sizeof f);
	return f;
}

int
main(void)
{
	float one = 1.0f;
	float one_ulp = from_bits(0x3f800001u);	/* 1 + 1 ULP          */
	float beyond = 1.01f;			/* ~84,000 ULP        */
	float tiny = from_bits(0x00000001u);	/* smallest subnormal  */
	double tol = harness_float_tol();
	int bad = 0, checks = 0;

	/*
	 * Expected failures must not spam stderr; diff_failures still counts
	 * them.  -1 rather than 0: the reporters print their one "suppressed"
	 * notice on `diff_failures == diff_max_report`, and 0 would let that
	 * through on every expected failure.
	 */
	diff_max_report = -1;

	/* --- exact: must pass in BOTH builds ------------------------------ */
	diff_begin("t_float_tol: exact");
	diff_eq_float("exact", one, one, 0);
	checks++;
	if (diff_failures != 0) {
		fprintf(stderr, "t_float_tol: exact compare failed\n");
		bad++;
	}

	/* --- 1 ULP: build-dependent --------------------------------------- */
	diff_begin("t_float_tol: one ULP");
	diff_eq_float("1ulp", one, one_ulp, 0);
	checks++;
	if (tol > 0.0) {
		if (diff_failures != 0) {
			fprintf(stderr, "t_float_tol: 1-ULP pair should pass "
				"under HARNESS_FLOAT_TOL\n");
			bad++;
		}
		if (diff_float_tolerant != 1) {
			fprintf(stderr, "t_float_tol: 1-ULP pass was not "
				"counted as tolerance-only (%d)\n",
				diff_float_tolerant);
			bad++;
		}
	} else {
		if (diff_failures != 1) {
			fprintf(stderr, "t_float_tol: 1-ULP pair must FAIL "
				"without HARNESS_FLOAT_TOL (failures=%d)\n",
				diff_failures);
			bad++;
		}
		if (diff_float_tolerant != 0) {
			fprintf(stderr, "t_float_tol: tolerance counter moved "
				"in the period-style build (%d)\n",
				diff_float_tolerant);
			bad++;
		}
	}

	/* --- beyond eps: must FAIL in BOTH builds ------------------------- */
	diff_begin("t_float_tol: beyond eps");
	diff_eq_float("beyond", one, beyond, 0);
	checks++;
	if (diff_failures != 1) {
		fprintf(stderr, "t_float_tol: beyond-eps pair must FAIL "
			"(failures=%d)\n", diff_failures);
		bad++;
	}
	if (diff_float_tolerant != 0) {
		fprintf(stderr, "t_float_tol: beyond-eps pair counted as "
			"tolerance-only\n");
		bad++;
	}

	/* --- relative, not absolute: must FAIL in BOTH builds ------------- */
	diff_begin("t_float_tol: no absolute slack");
	diff_eq_float("near-zero", 0.0f, tiny, 0);
	checks++;
	if (diff_failures != 1) {
		fprintf(stderr, "t_float_tol: a difference near zero must "
			"FAIL (the criterion is relative, not absolute)\n");
		bad++;
	}

	printf("%s t_float_tol: %d checks, %d bad (linked harness tol=%g)\n",
	       bad == 0 ? "PASS" : "FAIL", checks, bad, tol);
	return bad == 0 ? 0 : 1;
}

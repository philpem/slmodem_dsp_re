/*
 * t_field_typed.c -- negative control for the field-typed object comparison.
 *
 * THE PROBLEM THIS SOLVES.  A raw `diff_eq_obj` byte compare cannot tell a
 * rounding-level float difference (which the modern tier's HARNESS_FLOAT_TOL
 * must reach) from a wrong index, flag or decision (which must stay a hard
 * failure).  `diff_eq_obj_float_` names the object's float spans, so only
 * those go through the tolerance-aware `diff_eq_float` while every other byte
 * stays exact.  That is a detector, and a detector that cannot fail is dead
 * (findings F134, F2400, F2401), so this binary proves BOTH directions:
 *
 *   a float perturbed by 1 ULP        must PASS under HARNESS_FLOAT_TOL and
 *                                     FAIL without it;
 *   a float perturbed beyond eps      must FAIL in BOTH builds;
 *   a changed index                   must FAIL in BOTH builds;
 *   a changed flag bit                must FAIL in BOTH builds;
 *   a changed non-float tail word     must FAIL in BOTH builds;
 *   a float array element beyond eps  must FAIL in BOTH builds.
 *
 * WHICH ARM IT IS IS ASKED OF THE LINKED HARNESS (`harness_float_tol`), not of
 * this file's own preprocessor, so a translation unit built without the
 * define cannot disagree with the object it calls.  It links only the
 * comparison support: no reconstruction, no blob.
 */

#include <stdio.h>
#include <string.h>

#include "harness.h"

struct field_typed_sample {
	float f;		/* +0  a float field      */
	unsigned idx;		/* +4  an index/decision  */
	unsigned flags;		/* +8  a flag word        */
	float arr[3];		/* +12 a float array      */
	unsigned tail;		/* +24 a non-float tail   */
	double d;		/* +28 a double field     */
};

/*
 * Sorted, non-overlapping, and only the float/double fields.  Everything else
 * in the object is compared exactly.
 */
static const struct diff_float_span spans[] = {
	{ 0, 1, 4 },		/* f    */
	{ 12, 3, 4 },		/* arr  */
	{ 28, 1, 8 },		/* d    */
};

static float
from_bits(unsigned u)
{
	float f;

	memcpy(&f, &u, sizeof f);
	return f;
}

static double
from_bits64(unsigned long long u)
{
	double d;

	memcpy(&d, &u, sizeof d);
	return d;
}

/* One field-typed compare; returns the failure count it produced. */
static int
run_spans(const struct field_typed_sample *a,
	  const struct field_typed_sample *b,
	  const struct diff_float_span *sp, size_t nsp)
{
	diff_begin("t_field_typed");
	diff_max_report = -1;
	diff_eq_obj_float_(__FILE__, __LINE__, "field typed",
			   "struct field_typed_sample", a, b, sizeof *a,
			   sp, nsp, 0);
	return diff_failures;
}

static int
run(const struct field_typed_sample *a, const struct field_typed_sample *b)
{
	return run_spans(a, b, spans, sizeof spans / sizeof spans[0]);
}

int
main(void)
{
	struct field_typed_sample a, b;
	double tol = harness_float_tol();
	int bad = 0, checks = 0;

	memset(&a, 0, sizeof a);
	memset(&b, 0, sizeof b);
	a.f = 1.0f;
	b.f = 1.0f;
	a.idx = 7u;
	b.idx = 7u;
	a.flags = 0x40u;
	b.flags = 0x40u;
	a.arr[0] = -2.0f;
	b.arr[0] = -2.0f;
	a.arr[1] = 3.5f;
	b.arr[1] = 3.5f;
	a.arr[2] = 0.0f;
	b.arr[2] = 0.0f;
	a.tail = 0xdeadbeefu;
	b.tail = 0xdeadbeefu;
	a.d = 1.0;
	b.d = 1.0;

	/* --- identical: must pass in BOTH builds -------------------------- */
	checks++;
	if (run(&a, &b) != 0) {
		fprintf(stderr, "t_field_typed: identical object failed\n");
		bad++;
	}

	/* --- 1 ULP float: build-dependent --------------------------------- */
	b.f = from_bits(0x3f800001u);		/* 1.0f + 1 ULP */
	checks++;
	if (tol > 0.0) {
		if (run(&a, &b) != 0) {
			fprintf(stderr, "t_field_typed: 1-ULP float should "
				"pass under HARNESS_FLOAT_TOL\n");
			bad++;
		}
		if (diff_float_tolerant == 0) {
			fprintf(stderr, "t_field_typed: the 1-ULP pass was not "
				"counted as tolerance-only\n");
			bad++;
		}
	} else {
		if (run(&a, &b) != 1) {
			fprintf(stderr, "t_field_typed: 1-ULP float must FAIL "
				"without HARNESS_FLOAT_TOL\n");
			bad++;
		}
	}
	b.f = 1.0f;

	/*
	 * A float NOT named in the spans must stay bit-exact even under the
	 * tolerance: the helper relaxes the float fields the caller NAMES and
	 * nothing else.  This is what stops it from being an off switch.
	 */
	b.f = from_bits(0x3f800001u);
	checks++;
	if (run_spans(&a, &b, 0, 0) != 1) {
		fprintf(stderr, "t_field_typed: an UNNAMED float must stay "
			"exact under HARNESS_FLOAT_TOL\n");
		bad++;
	}
	b.f = 1.0f;

	/* --- float beyond eps: must FAIL in BOTH builds ------------------- */
	b.f = 1.01f;				/* ~84,000 ULP */
	checks++;
	if (run(&a, &b) != 1) {
		fprintf(stderr, "t_field_typed: beyond-eps float must FAIL\n");
		bad++;
	}
	b.f = 1.0f;

	/* --- changed index: must FAIL in BOTH builds ---------------------- */
	b.idx = 8u;
	checks++;
	if (run(&a, &b) != 1) {
		fprintf(stderr, "t_field_typed: a changed index must FAIL\n");
		bad++;
	}
	b.idx = 7u;

	/* --- changed flag bit: must FAIL in BOTH builds ------------------- */
	b.flags = 0x41u;
	checks++;
	if (run(&a, &b) != 1) {
		fprintf(stderr, "t_field_typed: a changed flag must FAIL\n");
		bad++;
	}
	b.flags = 0x40u;

	/* --- changed non-float tail: must FAIL in BOTH builds ------------- */
	b.tail = 0xdeadbeee;
	checks++;
	if (run(&a, &b) != 1) {
		fprintf(stderr, "t_field_typed: a changed tail word must "
			"FAIL\n");
		bad++;
	}
	b.tail = 0xdeadbeefu;

	/* --- float array element beyond eps: must FAIL in BOTH builds ----- */
	b.arr[1] = 3.6f;
	checks++;
	if (run(&a, &b) != 1) {
		fprintf(stderr, "t_field_typed: a float-array element beyond "
			"eps must FAIL\n");
		bad++;
	}
	b.arr[1] = 3.5f;

	/* --- float array element 1 ULP: build-dependent ------------------- */
	b.arr[1] = from_bits(0x40600001u);	/* 3.5f + 1 ULP */
	checks++;
	if (tol > 0.0) {
		if (run(&a, &b) != 0) {
			fprintf(stderr, "t_field_typed: a 1-ULP array element "
				"should pass under HARNESS_FLOAT_TOL\n");
			bad++;
		}
	} else {
		if (run(&a, &b) != 1) {
			fprintf(stderr, "t_field_typed: a 1-ULP array element "
				"must FAIL without HARNESS_FLOAT_TOL\n");
			bad++;
		}
	}
	b.arr[1] = 3.5f;

	/* --- double 1 ULP: build-dependent -------------------------------- */
	b.d = from_bits64(0x3ff0000000000001ULL);	/* 1.0 + 1 ULP */
	checks++;
	if (tol > 0.0) {
		if (run(&a, &b) != 0) {
			fprintf(stderr, "t_field_typed: a 1-ULP double should "
				"pass under HARNESS_FLOAT_TOL\n");
			bad++;
		}
		if (diff_float_tolerant == 0) {
			fprintf(stderr, "t_field_typed: the 1-ULP double pass "
				"was not counted as tolerance-only\n");
			bad++;
		}
	} else {
		if (run(&a, &b) != 1) {
			fprintf(stderr, "t_field_typed: a 1-ULP double must "
				"FAIL without HARNESS_FLOAT_TOL\n");
			bad++;
		}
	}
	b.d = 1.0;

	/* --- double beyond eps: must FAIL in BOTH builds ------------------ */
	b.d = 1.01;
	checks++;
	if (run(&a, &b) != 1) {
		fprintf(stderr, "t_field_typed: a beyond-eps double must "
			"FAIL\n");
		bad++;
	}
	b.d = 1.0;

	printf("%s t_field_typed: %d checks, %d bad (linked harness tol=%g)\n",
	       bad == 0 ? "PASS" : "FAIL", checks, bad, tol);
	return bad == 0 ? 0 : 1;
}

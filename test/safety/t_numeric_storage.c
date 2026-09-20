/* Shared numeric/storage apparatus controls, portable across ILP32/LP64.
 * Expected failures must have their exact failure count; controls are not
 * ordinary differential fixtures and do not enlarge the period binary set. */
#include <stdint.h>
#include <limits.h>
#include <float.h>
#include "harness.h"

static int controls, errors, modern;
static float fbits(uint32_t u) { float f; memcpy(&f, &u, 4); return f; }
static double dbits(uint64_t u) { double d; memcpy(&d, &u, 8); return d; }
static void expect(const char *name, int want)
{
	++controls;
	if (diff_failures != want) {
		++errors;
		printf("control %s: expected %d failures, got %d\n", name, want, diff_failures);
	}
	diff_begin("numeric storage control");
}
static void double_case(const char *name, double a, double b, int bad)
{
	diff_eq_double(name, a, b, controls);
	expect(name, bad);
}
static void invalid(const struct diff_float_span *sp, size_t count, size_t n)
{
	unsigned char a[16] = { 0 }, b[16] = { 0 };
	b[4] = 1;
	diff_eq_obj_float_(__FILE__, __LINE__, "invalid span", "bytes", a, b, n, sp, count, controls);
	/* One descriptor failure, no partial object comparisons before it. */
	if (diff_checks != 1) { ++errors; printf("invalid span compared a prefix\n"); }
	expect("invalid descriptor", 1);
}
int main(void)
{
	double pinf = dbits(0x7ff0000000000000ULL), ninf = dbits(0xfff0000000000000ULL);
	double nan1 = dbits(0x7ff8000000000001ULL), nan2 = dbits(0x7ff8000000000002ULL);
	unsigned char a[16] = { 0 }, b[16] = { 0 };
	struct diff_float_span f = { 0, 1, 4 }, d = { 0, 1, 8 };
	struct diff_float_span bad[] = { { 4, 1, 4 }, { 0, 1, 0 },
		{ 0, 1, 1 }, { 0, 1, 2 }, { 0, 1, 16 }, { UINT_MAX, 1, 4 },
		{ 0, UINT_MAX, 8 } };
	struct diff_float_span overlap[] = { { 0, 2, 4 }, { 4, 1, 4 } };
	struct diff_float_span unsorted[] = { { 8, 1, 4 }, { 0, 1, 4 } };
	struct diff_float_span late[] = { { 0, 4, 4 }, { 16, 1, 4 } };
	uint32_t u;
	uint64_t v;
	int i, mode;
	modern = harness_float_tol() > 0;
	diff_begin("numeric storage control");
	diff_max_report = -1;
	for (mode = 0; mode < 2; ++mode) {
		if (mode) harness_float_tol_fixture_mixed(.01, 1e-6);
		else harness_float_tol_fixture(0);
		double_case("inf/finite", pinf, 1, 1);
		double_case("finite/inf", 1, pinf, 1);
		double_case("negative inf/finite", ninf, -1, 1);
		double_case("finite/negative inf", -1, ninf, 1);
		double_case("opposite infinities", pinf, ninf, 1);
		double_case("reversed infinities", ninf, pinf, 1);
		double_case("equal positive inf", pinf, pinf, 0);
		double_case("equal negative inf", ninf, ninf, 0);
		double_case("NaN payloads scalar", nan1, nan2, 0);
		double_case("NaN/finite", nan1, 1, 1);
		double_case("finite/NaN", 1, nan1, 1);
		double_case("extreme finite", DBL_MAX, -DBL_MAX, 1);
		double_case("extreme finite reversed", -DBL_MAX, DBL_MAX, 1);
		double_case("neighbor max finite", DBL_MAX, dbits(0x7feffffffffffffeULL), !modern);
	}
	harness_float_tol_fixture_mixed(DBL_MAX / 2, 1);
	double_case("overflow cannot excuse", DBL_MAX, -DBL_MAX, 1);
	double_case("overflow reverse cannot excuse", -DBL_MAX, DBL_MAX, 1);
	harness_float_tol_fixture_mixed(.25, 0);
	diff_eq_int("zero mixed relative policy", harness_float_tol() == 0, 1, 0);
	expect("zero relative policy", 0);
	double_case("absolute only outside", 1000001., 1000000., 1);
	double_case("absolute only inside", 1.125, 1., !modern);
	diff_eq_float("float absolute only outside", 1000001.f, 1000000.f, 0);
	expect("float absolute only outside", 1);
	harness_float_tol_fixture_mixed(0, .5);
	double_case("zero absolute uses reference", 2, 1, 1);
	diff_eq_float("float zero absolute uses reference", 2, 1, 0);
	expect("float zero absolute uses reference", 1);
	harness_float_tol_fixture(0);
	diff_eq_int("positive ulp", float_ulps(fbits(0x3f800000u), fbits(0x3f800001u)), 1, 0);
	diff_eq_int("negative ulp", float_ulps(fbits(0xbf800000u), fbits(0xbf800001u)), 1, 0);
	diff_eq_int("zero signs ulp", float_ulps(0, fbits(0x80000000u)), 0, 0);
	diff_eq_int("subnormal across zero", float_ulps(fbits(1), fbits(0x80000001u)), 2, 0);
	diff_eq_int("extreme float distance", float_ulps(FLT_MAX, -FLT_MAX) == 0xfefffffeUL, 1, 0);
	diff_eq_int("reverse float distance", float_ulps(-FLT_MAX, FLT_MAX) == 0xfefffffeUL, 1, 0);
	expect("fixed width ulp ordering (6 assertions)", 0);
	diff_eq_float("scalar NaNs retain semantics", fbits(0x7fc00001u), fbits(0x7fc00002u), 0);
	diff_eq_float("scalar zero signs retain semantics", 0, fbits(0x80000000u), 0);
	double_case("scalar double zero signs", 0, dbits(0x8000000000000000ULL), 0);
	for (i = 0; i < 7; ++i) invalid(&bad[i], 1, i ? 16 : 6);
	invalid(overlap, 2, 16); invalid(unsorted, 2, 16); invalid(late, 2, 16);
	invalid(0, 1, 16);
	for (i = 0; i < 4; ++i) {
		memset(a, 0, sizeof a); memset(b, 0, sizeof b);
		if (i == 0) { u = 0x80000000u; memcpy(a, &u, 4); }
		if (i == 1) { u = 0x7fc00001u; memcpy(a, &u, 4); ++u; memcpy(b, &u, 4); }
		if (i == 2) { v = 0x8000000000000000ULL; memcpy(a, &v, 8); }
		if (i == 3) { memcpy(a, &nan1, 8); memcpy(b, &nan2, 8); }
		diff_eq_obj_float_(__FILE__, __LINE__, "raw typed storage", "bytes", a, b, 16,
		                   i < 2 ? &f : &d, 1, i);
		expect("typed storage zero/NaN", !modern);
	}
	memset(a, 0, sizeof a); memset(b, 0, sizeof b); b[15] = 1;
	diff_eq_obj_float_(__FILE__, __LINE__, "exact tail", "bytes", a, b, 16, &f, 1, 0);
	expect("unmodelled tail", 1);
	printf("numeric/storage safety: %d controls, %d failed; %zu-bit long, %s\n",
	       controls, errors, sizeof(long) * 8, modern ? "modern" : "no-define");
	return errors != 0;
}

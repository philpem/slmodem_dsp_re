/* Direct safety controls of the apparatus used by all three graph fixtures.
 * Expected assertion failures are inspected, never returned as test success
 * without checking the exact failure count. Build with and without tolerance. */
#include <string.h>
#include "harness.h"

static struct region {
	unsigned char *a, *b;
	unsigned size, foff, fcount;
} reg[8];
static int nreg;
static unsigned char objects[2][8][128];
static unsigned char *base[2] = { objects[0][0], objects[1][0] };
#include "region_float_graph.h"

static int controls, errors;
static void expect(int failures)
{
	++controls;
	if (diff_failures != failures) {
		++errors;
		printf("control %d: expected %d failures, got %d\n",
		       controls, failures, diff_failures);
	}
	diff_begin("next apparatus control");
}

static void pointer(int parent, unsigned off, int child, int side)
{
	void *p = objects[side][child];
	memcpy(objects[side][parent] + off, &p, sizeof p);
}

int main(void)
{
	int i, s, modern = harness_float_tol() > 0;
	unsigned bits = 0x38d1b718u;
	float boundary, original = 0.369140625f, changed = 0.3691906333f;
	double saved = harness_float_tol();
	diff_begin("scoped word controls");
	diff_eq_float_word("signed zero", 0x80000000u, 0, 1e-4, 1e-6, 0);
	expect(modern ? 0 : 1);
	diff_eq_float_word("NaN payload", 0x7fc00001u, 0x7fc00002u, 1e-4, 1e-6, 0);
	expect(modern ? 0 : 1);
	diff_eq_int("budget restored", harness_float_atol() == 0 &&
	            harness_float_tol() == saved, 1, 0);
	expect(0);
	diff_eq_int("raw input integrity", memcmp(&original, &changed, 4) == 0, 1, 0);
	expect(1);
	diff_eq_float("ordinary output unchanged tolerance", changed, original, 0);
	expect(1);
	memcpy(&boundary, &bits, 4);
	harness_float_tol_fixture_mixed(1e-4, 1e-6);
	i = harness_float_within(boundary, 0);
	diff_eq_float("boundary", boundary, 0, 0);
	expect(i ? 0 : 1);
	diff_eq_int("reference-scaled boundary rejected", i, 0, 0);
	expect(0);
	harness_float_tol_fixture(0);
	diff_eq_float_word("within scoped allowance", 0x3f800001u, 0x3f800000u,
	                   1e-4, 1e-6, 0);
	expect(modern ? 0 : 1);
	diff_eq_float_word("outside scoped allowance", 0x40000000u, 0x3f800000u,
	                   1e-4, 1e-6, 0);
	expect(1);
	nreg = 8;
	for (i = 0; i < nreg; ++i) {
		reg[i].a = objects[0][i]; reg[i].b = objects[1][i]; reg[i].size = 128;
	}
	for (s = 0; s < 2; ++s) {
		pointer(0, 0x24, 1, s); pointer(0, 4, 2, s);
		pointer(2, 0x0c, 3, s);
		/* demod pointer and modem pointer absent on both sides */
	}
	mark_float_graph(0, 64, 80);
	expect(0);
	diff_eq_int("classified surface", classified_float_words, 66, 0);
	diff_eq_int("total surface", classified_total_words, 256, 0);
	expect(0);
	pointer(0, 0x24, 3, 1); /* same-sized, wrong side-B path */
	float_hop(0, 0x24);
	expect(1);
	float_hop(0, 128);
	expect(1);
	float_mark_pair(4, 124, 2);
	expect(1);
	float_mark_pair(4, 1, 1);
	expect(1);
	float_mark_pair(4, 0, ~0u);
	expect(1);
	reg[4].size = 127;
	float_mark_pair(4, 0, 0);
	expect(1);
	printf("region float safety: %d controls, %d failed, tier %s\n",
	       controls, errors, modern ? "modern" : "no-define");
	return errors != 0;
}

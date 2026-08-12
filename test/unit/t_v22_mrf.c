/*
 * t_v22_mrf.c -- differential test of the V.22 multi-rate filter.
 *
 * THE COEFFICIENT ARRAY IS AN OUTPUT, not just an input.  `V22_MRF_init`
 * permutes it in place, so the two sides cannot share one: each gets its own
 * writable copy of `MRFv22_COFFS` and the permuted results are compared
 * element by element.  `permuted()` asserts the copy actually moved, so a
 * reconstruction that forgot the permutation entirely could not pass by
 * having both sides leave the array alone.
 *
 * The history buffer is compared by CONTENT over `[0 .. widx - 1]`.  That is
 * the region both sides have written; `[widx .. 59]` is whatever malloc
 * returned and differs by construction.  The buffer is where the memcpy-down
 * lives, so an off-by-one in the slide would otherwise only show up as an
 * output difference several samples later.
 *
 * `phase`, `widx` and `need` persist across calls, so the stream is driven
 * three ways -- one big call, ragged chunks, and one sample at a time.  With
 * a 9:20 ratio `need` alternates between 2 and 3, so single-sample calls
 * cross the partial-consume path on every call and never once complete an
 * output from a full `need`.
 */

#include <string.h>

#include "harness.h"
#include "dsplib/v22_mrf.h"

extern void ref_V22_MRF_init(void *state, const void *cfg, int fresh);
extern void ref_V22_MRF_free(void *state);
extern short ref_V22_MRF_filter(void *state, const short *in, short *out,
				short count);
extern const short ref_MRFv22_COFFS[];
extern struct v22_mrf_cfg ref_V22_MRF_CFG;

#define NSAMP 3000
static short input[NSAMP];
static short oa[NSAMP], ob[NSAMP];
static short ca[V22_MRF_COEFFS], cb[V22_MRF_COEFFS];

static void
make_input(void)
{
	unsigned lfsr = 0x5C3B7Au;
	int i;

	for (i = 0; i < NSAMP; i++) {
		lfsr = (lfsr >> 1) ^ (-(int)(lfsr & 1u) & 0xB400u);
		input[i] = (short)((int)(lfsr & 0x3fff) - 0x2000
				   + 6000 * ((i % 13) < 7 ? 1 : -1));
	}
}

/* Did init actually rewrite the array?  Otherwise the test proves nothing. */
static int
permuted(const short *c)
{
	int i;

	for (i = 0; i < V22_MRF_COEFFS; i++)
		if (c[i] != ref_MRFv22_COFFS[i])
			return 1;
	return 0;
}

static void
load_coeffs(void)
{
	memcpy(ca, ref_MRFv22_COFFS, sizeof(ca));
	memcpy(cb, ref_MRFv22_COFFS, sizeof(cb));
}

static void
compare_state(const struct v22_mrf *b, const struct v22_mrf *a, long where)
{
	int i;

	diff_eq_int("at %ld: need", b->need, a->need, where);
	diff_eq_int("at %ld: phase", b->phase, a->phase, where);
	diff_eq_int("at %ld: widx", b->widx, a->widx, where);
	diff_eq_int("at %ld: history_len", b->history_len, a->history_len,
		    where);
	diff_eq_int("at %ld: cfg.aux", (long)b->cfg.aux, (long)a->cfg.aux,
		    where);

	/* The two buffers are two allocations, so only the written part. */
	for (i = 0; i < b->widx && i < a->widx; i++)
		diff_eq_int("at %ld: history[]", b->history[i], a->history[i],
			    (long)i);
}

static int
tables(void)
{
	int i;

	diff_begin("v22 mrf tables");
	for (i = 0; i < V22_MRF_COEFFS; i++)
		diff_eq_int("MRFv22_COFFS[%ld]", MRFv22_COFFS[i],
			    ref_MRFv22_COFFS[i], i);
	diff_eq_obj("V22_MRF_CFG", struct v22_mrf_cfg, &V22_MRF_CFG,
		    &ref_V22_MRF_CFG, 0);
	return diff_end();
}

/*
 * init's two paths.  `fresh` allocates; anything else adopts the pointer
 * already in the state, which is why the reuse case has to follow a fresh one
 * on the same object rather than standing alone.
 */
static int
setup(void)
{
	struct v22_mrf a, b;
	struct v22_mrf_cfg cfga, cfgb;
	int i, rc;

	load_coeffs();
	memset(&a, 0, sizeof(a));
	memset(&b, 0, sizeof(b));
	cfga.coeff = ca;
	cfga.aux = 0;
	cfgb.coeff = cb;
	cfgb.aux = 0;

	diff_begin("v22 mrf init");

	ref_V22_MRF_init(&a, &cfga, 1);
	V22_MRF_init(&b, &cfgb, 1);

	diff_eq_int("fresh init permuted the array (%ld)", permuted(cb),
		    1, 0);
	for (i = 0; i < V22_MRF_COEFFS; i++)
		diff_eq_int("fresh init: coeff[%ld]", cb[i], ca[i], i);
	diff_eq_int("fresh init: cfg.coeff kept (%ld)",
		    b.cfg.coeff == cb, a.cfg.coeff == ca, 0);
	compare_state(&b, &a, 0);
	for (i = 0; i < b.history_len; i++)
		diff_eq_int("fresh init: history[%ld]", b.history[i],
			    a.history[i], i);

	/* Reuse: same buffers, and the array gets permuted a second time. */
	ref_V22_MRF_init(&a, &cfga, 0);
	V22_MRF_init(&b, &cfgb, 0);

	for (i = 0; i < V22_MRF_COEFFS; i++)
		diff_eq_int("reuse init: coeff[%ld]", cb[i], ca[i], i);
	compare_state(&b, &a, 1);
	for (i = 0; i < b.history_len; i++)
		diff_eq_int("reuse init: history[%ld]", b.history[i],
			    a.history[i], i);

	rc = diff_end();
	ref_V22_MRF_free(&a);
	V22_MRF_free(&b);
	return rc;
}

static int
run(const char *label, const int *chunks, int nchunks)
{
	struct v22_mrf a, b;
	struct v22_mrf_cfg cfga, cfgb;
	int pos = 0, na = 0, nb = 0, ci = 0, rc;

	load_coeffs();
	memset(&a, 0, sizeof(a));
	memset(&b, 0, sizeof(b));
	cfga.coeff = ca;
	cfga.aux = 0;
	cfgb.coeff = cb;
	cfgb.aux = 0;
	ref_V22_MRF_init(&a, &cfga, 1);
	V22_MRF_init(&b, &cfgb, 1);

	diff_begin(label);

	/* An empty call must be a no-op on both sides, not a stall. */
	diff_eq_int("empty call returns %ld", V22_MRF_filter(&b, input, ob, 0),
		    ref_V22_MRF_filter(&a, input, oa, 0), 0);
	compare_state(&b, &a, -1);

	while (pos < NSAMP) {
		int n = chunks[ci++ % nchunks];
		short cra, crb;
		int k;

		if (pos + n > NSAMP)
			n = NSAMP - pos;

		cra = ref_V22_MRF_filter(&a, input + pos, oa + na, (short)n);
		crb = V22_MRF_filter(&b, input + pos, ob + nb, (short)n);

		diff_eq_int("at %ld: output count", crb, cra, pos);
		if (cra != crb)
			break;
		for (k = 0; k < cra; k++)
			diff_eq_int("sample %ld", ob[nb + k], oa[na + k],
				    na + k);
		compare_state(&b, &a, pos);

		na += cra;
		nb += crb;
		pos += n;
	}
	diff_eq_int("total outputs (%ld)", nb, na, 0);

	rc = diff_end();
	ref_V22_MRF_free(&a);
	V22_MRF_free(&b);
	return rc;
}

int
main(void)
{
	static const int bulk[] = { 480 };
	static const int ragged[] = { 7, 1, 53, 2, 160, 11, 3, 97 };
	static const int tiny[] = { 1 };
	static const int pair[] = { 2 };
	int rc = 0;

	make_input();

	rc |= tables();
	rc |= setup();
	rc |= run("v22 mrf 9:20 bulk", bulk, 1);
	rc |= run("v22 mrf 9:20 ragged", ragged,
		  sizeof(ragged) / sizeof(ragged[0]));
	rc |= run("v22 mrf 9:20 one-at-a-time", tiny, 1);
	rc |= run("v22 mrf 9:20 two-at-a-time", pair, 1);
	return rc;
}

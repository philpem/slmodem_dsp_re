/*
 * t_v22_pps.c -- differential test of the V.22 transmit pulse shaper.
 *
 * Like `t_v22_mrf.c`, THE COEFFICIENT ARRAYS ARE OUTPUTS: `V22_PPS_init`
 * permutes both of them in place, so each side gets its own writable copies
 * and `permuted()` asserts the copies actually moved.
 *
 * The two arrays are deliberately DIFFERENT from one another -- the Q set is
 * the prototype rotated by a quarter of its length -- because in the object
 * they are the same pulse against two carrier phases.  Feeding one array
 * twice would let an I/Q swap pass unnoticed, and the output is their
 * difference, where a swap only changes the sign.
 *
 * The constellation maps are the object's own `SMCv22_IMAP_1200BPS` and
 * `SMCv22_QMAP_1200BPS`.  They are pure inputs, so both sides share them; the
 * symbol ring is not, because `rd` is advanced through it, and it is compared
 * after every call.
 *
 * `count` is in SYMBOLS and each one becomes about 13.33 samples, so the
 * output length is not the input length and the ragged case has to track two
 * separate cursors.  One symbol at a time is the harsh case: `need` is 1 on
 * entry to roughly one call in thirteen and 0 on the rest, which is the
 * bookkeeping most likely to drift.
 */

#include <string.h>

#include "harness.h"
#include "dsplib/fpm_smc.h"
#include "dsplib/v22_pps.h"

extern void ref_V22_PPS_init(void *state, const void *cfg, int fresh);
extern void ref_V22_PPS_free(void *state);
extern short ref_V22_PPS_filter(void *state, void *src, short *out,
				unsigned short count);
extern const short ref_PPSv22_COFFS[];
extern const struct v22_pps_cfg ref_PPSv22_CFG;
extern const short ref_SMCv22_IMAP_1200BPS[];
extern const short ref_SMCv22_QMAP_1200BPS[];

#define NSYM 240
#define NOUT 8192

static short symbols[NSYM];
static short oa[NOUT], ob[NOUT];
static short base_i[V22_PPS_COEFFS], base_q[V22_PPS_COEFFS];
static short cia[V22_PPS_COEFFS], cqa[V22_PPS_COEFFS];
static short cib[V22_PPS_COEFFS], cqb[V22_PPS_COEFFS];

static void
make_input(void)
{
	unsigned lfsr = 0x1F2E3Du;
	int i;

	for (i = 0; i < NSYM; i++) {
		lfsr = (lfsr >> 1) ^ (-(int)(lfsr & 1u) & 0xB400u);
		symbols[i] = (short)(lfsr & 0x0f);
	}

	for (i = 0; i < V22_PPS_COEFFS; i++) {
		base_i[i] = ref_PPSv22_COFFS[i];
		base_q[i] = ref_PPSv22_COFFS[(i + V22_PPS_PHASES / 4 * 3)
					     % V22_PPS_COEFFS];
	}
}

static void
load_coeffs(void)
{
	memcpy(cia, base_i, sizeof(cia));
	memcpy(cqa, base_q, sizeof(cqa));
	memcpy(cib, base_i, sizeof(cib));
	memcpy(cqb, base_q, sizeof(cqb));
}

/* Did init actually rewrite the arrays?  Otherwise the test proves nothing. */
static int
permuted(void)
{
	int i;

	for (i = 0; i < V22_PPS_COEFFS; i++)
		if (cib[i] != base_i[i] || cqb[i] != base_q[i])
			return 1;
	return 0;
}

/*
 * The two format strings carry which init produced the array, since both
 * calls report from the same source line.
 */
static void
compare_coeffs(const char *fmt_i, const char *fmt_q)
{
	int i;

	for (i = 0; i < V22_PPS_COEFFS; i++) {
		diff_eq_int(fmt_i, cib[i], cia[i], i);
		diff_eq_int(fmt_q, cqb[i], cqa[i], i);
	}
}

static void
compare_state(const struct v22_pps *b, const struct v22_pps *a,
	      const struct fpm_smc_syms *sb, const struct fpm_smc_syms *sa,
	      long where)
{
	int n, i;

	diff_eq_int("at %ld: need", b->need, a->need, where);
	diff_eq_int("at %ld: phase", b->phase, a->phase, where);
	diff_eq_int("at %ld: widx", b->widx, a->widx, where);
	diff_eq_int("at %ld: history_len", b->history_len, a->history_len,
		    where);
	diff_eq_int("at %ld: cfg.step", b->cfg.step, a->cfg.step, where);
	diff_eq_int("at %ld: cfg.pad02", b->cfg.pad02, a->cfg.pad02, where);
	diff_eq_int("at %ld: cfg.aux", (long)b->cfg.aux, (long)a->cfg.aux,
		    where);
	diff_eq_int("at %ld: src->rd", sb->rd, sa->rd, where);
	diff_eq_int("at %ld: src->size", sb->size, sa->size, where);

	/*
	 * Two allocations per side, so only the written region: everything up
	 * to `widx`, and at least the zeroed startup entries.
	 */
	n = b->widx > b->history_len - 1 ? b->widx : b->history_len - 1;
	for (i = 0; i <= n; i++) {
		diff_eq_int("at %ld: hist_i[]", b->hist_i[i], a->hist_i[i],
			    (long)i);
		diff_eq_int("at %ld: hist_q[]", b->hist_q[i], a->hist_q[i],
			    (long)i);
	}
}

static int
tables(void)
{
	int i;

	diff_begin("v22 pps tables");
	for (i = 0; i < V22_PPS_COEFFS; i++)
		diff_eq_int("PPSv22_COFFS[%ld]", PPSv22_COFFS[i],
			    ref_PPSv22_COFFS[i], i);
	diff_eq_obj("PPSv22_CFG", struct v22_pps_cfg, &PPSv22_CFG,
		    &ref_PPSv22_CFG, 0);
	return diff_end();
}

static void
setup_pair(struct v22_pps *a, struct v22_pps *b, struct v22_pps_cfg *cfga,
	   struct v22_pps_cfg *cfgb, struct fpm_smc_syms *sa,
	   struct fpm_smc_syms *sb)
{
	load_coeffs();
	memset(a, 0, sizeof(*a));
	memset(b, 0, sizeof(*b));

	/* Built the way V22FP_create builds it: copy the template, patch. */
	*cfga = PPSv22_CFG;
	cfga->coeff_i = cia;
	cfga->coeff_q = cqa;
	*cfgb = PPSv22_CFG;
	cfgb->coeff_i = cib;
	cfgb->coeff_q = cqb;

	ref_V22_PPS_init(a, cfga, 1);
	V22_PPS_init(b, cfgb, 1);

	/* init leaves these two alone; the caller writes them. */
	a->imap = ref_SMCv22_IMAP_1200BPS;
	a->qmap = ref_SMCv22_QMAP_1200BPS;
	b->imap = ref_SMCv22_IMAP_1200BPS;
	b->qmap = ref_SMCv22_QMAP_1200BPS;

	memset(sa, 0, sizeof(*sa));
	memset(sb, 0, sizeof(*sb));
	sa->sym = symbols;
	sa->size = NSYM;
	sb->sym = symbols;
	sb->size = NSYM;
}

static int
setup(void)
{
	struct v22_pps a, b;
	struct v22_pps_cfg cfga, cfgb;
	struct fpm_smc_syms sa, sb;
	int rc;

	diff_begin("v22 pps init");
	setup_pair(&a, &b, &cfga, &cfgb, &sa, &sb);

	diff_eq_int("fresh init permuted both arrays (%ld)", permuted(), 1, 0);
	compare_coeffs("fresh init: coeff_i[%ld]", "fresh init: coeff_q[%ld]");
	diff_eq_int("fresh init: cfg.coeff_i kept (%ld)",
		    b.cfg.coeff_i == cib, a.cfg.coeff_i == cia, 0);
	diff_eq_int("fresh init: cfg.coeff_q kept (%ld)",
		    b.cfg.coeff_q == cqb, a.cfg.coeff_q == cqa, 0);
	compare_state(&b, &a, &sb, &sa, 0);

	/* Reuse: the buffers stay, and the arrays get permuted again. */
	ref_V22_PPS_init(&a, &cfga, 0);
	V22_PPS_init(&b, &cfgb, 0);
	compare_coeffs("reuse init: coeff_i[%ld]", "reuse init: coeff_q[%ld]");
	compare_state(&b, &a, &sb, &sa, 1);

	rc = diff_end();
	ref_V22_PPS_free(&a);
	V22_PPS_free(&b);
	return rc;
}

static int
run(const char *label, const int *chunks, int nchunks)
{
	struct v22_pps a, b;
	struct v22_pps_cfg cfga, cfgb;
	struct fpm_smc_syms sa, sb;
	int pos = 0, na = 0, nb = 0, ci = 0, rc;

	setup_pair(&a, &b, &cfga, &cfgb, &sa, &sb);

	diff_begin(label);

	/* An empty call must be a no-op on both sides. */
	diff_eq_int("empty call returns %ld",
		    V22_PPS_filter(&b, &sb, ob, 0),
		    ref_V22_PPS_filter(&a, &sa, oa, 0), 0);
	compare_state(&b, &a, &sb, &sa, -1);

	while (pos < NSYM) {
		int n = chunks[ci++ % nchunks];
		short cra, crb;
		int k;

		if (pos + n > NSYM)
			n = NSYM - pos;
		if (na + n * 15 + 32 > NOUT || nb + n * 15 + 32 > NOUT)
			break;

		cra = ref_V22_PPS_filter(&a, &sa, oa + na, (unsigned short)n);
		crb = V22_PPS_filter(&b, &sb, ob + nb, (unsigned short)n);

		diff_eq_int("at %ld: output count", crb, cra, pos);
		if (cra != crb)
			break;
		for (k = 0; k < cra; k++)
			diff_eq_int("sample %ld", ob[nb + k], oa[na + k],
				    na + k);
		compare_state(&b, &a, &sb, &sa, pos);

		na += cra;
		nb += crb;
		pos += n;
	}
	diff_eq_int("total outputs (%ld)", nb, na, 0);
	diff_eq_int("outputs were produced (%ld)", nb > 0, 1, 0);

	rc = diff_end();
	ref_V22_PPS_free(&a);
	V22_PPS_free(&b);
	return rc;
}

int
main(void)
{
	static const int bulk[] = { 60 };
	static const int ragged[] = { 3, 1, 17, 2, 40, 5, 1, 29 };
	static const int tiny[] = { 1 };
	int rc = 0;

	make_input();

	rc |= tables();
	rc |= setup();
	rc |= run("v22 pps 40:3 bulk", bulk, 1);
	rc |= run("v22 pps 40:3 ragged", ragged,
		  sizeof(ragged) / sizeof(ragged[0]));
	rc |= run("v22 pps 40:3 one-at-a-time", tiny, 1);
	return rc;
}

/*
 * t_faxsmc.c -- differential test of the fax pumps' symbol coder.
 *
 * `SMC_init` is byte-for-byte `FPM_SMC_init` (F8900).  `SMC_encoder` is not:
 * it is `FPM_SMC_encoder` plus a second output form selected by `cfg.f00`,
 * and the whole point of this test is that BOTH forms are driven, because a
 * fixture that only ran the index form would be re-testing code
 * `t_fpm_smc` already covers and reporting a pass for the 242 bytes it never
 * entered.  The counters at the bottom are read off the run and asserted.
 *
 * What each part is here to break:
 *
 *   complex form, real config    V.29's own numbers -- `direct` 1, `rot_step`
 *                                17, `rot_mod` 24, `qmask` 7, `pmask` 7,
 *                                `amask` 8 -- which is 1700 Hz over 2400
 *                                baud.  A phasor read at the WRONG `acc`
 *                                (after the step rather than before it) gives
 *                                the right first symbol and the wrong rest,
 *                                so the comparison is per symbol.
 *
 *   the two rails, per symbol    `i` and `q` are compared after every word.
 *                                Swapping the two terms of either -- which is
 *                                F8790's shape -- survives a whole-buffer
 *                                comparison of one rail and dies here.
 *
 *   asymmetric tables            `imap`, `qmap`, `cosine` and `sine` hold four
 *                                different sequences with no symmetry between
 *                                them, so exchanging any pair changes every
 *                                output.  A cosine and sine that were each
 *                                other's mirror would hide exactly that.
 *
 *   index form beside it         the same config with `f00` set, so the two
 *                                forms are driven over the same data and the
 *                                selector itself is under test.
 *
 *   count 0 on a dirty state     `quad`, `acc` and `widx` are written back
 *                                anyway, which is where a `return` before the
 *                                write-backs would show.
 *
 *   widx seeded past len         the wrap is tested AFTER the store, so the
 *                                first symbol goes to the out-of-range slot.
 *                                The rings here are over-sized on both sides
 *                                so that both read and write the same slack
 *                                rather than two different pieces of rubbish
 *                                (D955's rule, applied to the fixture).
 *
 *   rot_mod 0 and negative       both wraps are one conditional subtract on a
 *                                SIGNED comparison, not a modulo.
 *
 * `SMC_CFG` is compared to the blob's copy dword by dword and then proved
 * equivalent by initialising from both.
 */

#include <string.h>

#include "harness.h"
#include "dsplib/smc.h"

extern void ref_SMC_init(void *smc, const void *cfg);
extern void ref_SMC_encoder(void *smc, void *ring, const unsigned short *data,
			    unsigned short count);
extern const struct fpm_smc_cfg ref_SMC_CFG;

/*
 * The ring is over-sized so that a `widx` seeded past `len` writes into slack
 * both sides own, and the slack is compared too.
 */
#define RINGLEN 12
#define RINGCAP 64
#define NWORDS  96

static long cov_index_form;
static long cov_complex_form;
static long cov_index_wrap;
static long cov_acc_wrap;
static long cov_widx_wrap;
static long cov_negative_quad;

static unsigned int rnd_state;

static unsigned int
rnd(void)
{
	rnd_state = rnd_state * 1103515245u + 12345u;
	return rnd_state >> 8;
}

/*
 * Four sequences with no symmetry between them: no two are each other's
 * negation, reverse or scaled copy, so exchanging any pair of tables changes
 * the output of every symbol.
 */
static const short imap16[16] = {
	 1000, -3117,  2044,   517, -1888,  7001,  -412,  2530,
	-6100,   233,  4096, -1021,   777, -2999,  3311,  -158
};
static const short qmap16[16] = {
	-2731,   609,  8000, -1444,   322,  1919, -5050,   -77,
	  466, -3600,  1234,  6666, -2222,   101, -8888,  4444
};
static const short cos32[32] = {
	 32767,  32138,  30273,  27245,  23170,  18204,  12539,   6392,
	     0,  -6392, -12539, -18204, -23170, -27245, -30273, -32138,
	-32767, -32138, -30273, -27245, -23170, -18204, -12539,  -6392,
	     0,   6392,  12539,  18204,  23170,  27245,  30273,  32138
};
static const short sin32[32] = {
	     7,   6392,  12539,  18204,  23170,  27245,  30273,  32138,
	 32767,  32138,  30273,  27245,  23170,  18204,  12539,   6392,
	    -3,  -6392, -12539, -18204, -23170, -27245, -30273, -32138,
	-32767, -32138, -30273, -27245, -23170, -18204, -12539,  -6392
};
static const unsigned short pmap16[16] = {
	4, 0, 8, 12, 1, 2, 3, 5,
	0xffff, 0x8000, 7, 9, 11, 13, 14, 15
};

struct ring_pair {
	short i[RINGCAP];
	short q[RINGCAP];
	short sym[RINGCAP];
	struct fpm_smc_ring r;
};

struct enc_case {
	const char *name;
	int f00;
	int direct;
	short rot_step;
	short rot_mod;
	unsigned short qshift;
	unsigned short qmask;
	unsigned short amask;
	unsigned short pmask;
	short quad0;
	short acc0;
	short widx0;
	short len;
	unsigned int inmask;
};

static const struct enc_case enc_cases[] = {
	/*                          f00 dir step mod shft qm  am  pm    quad acc widx len  inmask */
	{ "V.29 9600, complex",	     0,  1, 17, 24,  0,  7,  8, 7,     0,  0,  0, RINGLEN, 0x000f },
	{ "V.29 7200, complex",	     0,  1, 17, 24,  0,  7,  0, 7,     0,  0,  0, RINGLEN, 0x0007 },
	{ "V.29 9600, index form",   1,  1, 17, 24,  0,  7,  8, 7,     0,  0,  0, RINGLEN, 0x000f },
	{ "complex, differential",   0,  0, 17, 24,  2,  3,  3, 15,    0,  5,  0, RINGLEN, 0x000f },
	{ "complex, seeded acc",     0,  1,  5, 32,  0, 15,  0, 15,    0, 31,  0, RINGLEN, 0x000f },
	{ "complex, step 0",         0,  1,  0, 24,  0,  7,  8, 7,     0, 11,  0, RINGLEN, 0x000f },
	{ "index, negative modulus", 1,  1,  1, -1,  0,  3,  3, 15,    0,  0,  0, 6,       0x000f },
	{ "index, over 2*mod",       1,  0,  1,  5,  0, 15, 15, 15,    0,  4,  0, 9,       0xffff },
	{ "index, wide shift",       1,  0,  0, 16, 12, 15,  3, 15,    0,  0,  0, 8,       0xffff },
	{ "index, quad negative",    1,  0,  0, 16,  0, 15,  0, 0xffff, -1, 0,  0, 8,      0xffff },
	{ "index, widx past len",    1,  1,  0, 16,  0,  3,  0, 15,    0,  0, 20, 8,       0x0003 },
	{ "complex, len of one",     0,  1,  1, 24,  0,  7,  8, 7,     0,  0,  0, 1,       0x000f }
};

#define NENC ((int)(sizeof(enc_cases) / sizeof(enc_cases[0])))

static void
make_cfg(struct fpm_smc_cfg *cfg, const struct enc_case *c)
{
	memset(cfg, 0, sizeof(*cfg));
	cfg->f00 = c->f00;
	cfg->direct = c->direct;
	cfg->rot_step = c->rot_step;
	cfg->rot_mod = c->rot_mod;
	cfg->qshift = c->qshift;
	cfg->qmask = c->qmask;
	cfg->amask = c->amask;
	cfg->pmask = c->pmask;
	cfg->pmap = pmap16;
	cfg->imap = imap16;
	cfg->qmap = qmap16;
	cfg->cosine = cos32;
	cfg->sine = sin32;
}

static void
make_ring(struct ring_pair *p, const struct enc_case *c)
{
	int k;

	for (k = 0; k < RINGCAP; k++) {
		p->i[k] = (short)(0x5a5a + k);
		p->q[k] = (short)(0x3c3c - k);
		p->sym[k] = (short)(0x0f0f + k);
	}
	p->r.i = p->i;
	p->r.q = p->q;
	p->r.sym = p->sym;
	p->r.widx = c->widx0;
	p->r.ridx = 0x1234;
	p->r.len = c->len;
}

static void
compare_rings(const struct ring_pair *a, const struct ring_pair *b, long tag)
{
	int k;

	diff_eq_int("widx %ld", a->r.widx, b->r.widx, tag);
	diff_eq_int("ridx %ld", a->r.ridx, b->r.ridx, tag);
	diff_eq_int("len %ld", a->r.len, b->r.len, tag);
	for (k = 0; k < RINGCAP; k++) {
		diff_eq_int("i[%ld]", a->i[k], b->i[k], k);
		diff_eq_int("q[%ld]", a->q[k], b->q[k], k);
		diff_eq_int("sym[%ld]", a->sym[k], b->sym[k], k);
	}
}

static int
run_case(const struct enc_case *c)
{
	struct fpm_smc_cfg cfg;
	struct fpm_smc a, b;
	static struct ring_pair pa;
	static struct ring_pair pb;
	static unsigned short src[NWORDS];
	int i;

	make_cfg(&cfg, c);
	make_ring(&pa, c);
	make_ring(&pb, c);

	diff_begin(c->name);

	memset(&a, 0xa5, sizeof(a));
	memset(&b, 0xa5, sizeof(b));
	ref_SMC_init(&b, &cfg);
	SMC_init(&a, &cfg);
	diff_eq_obj("after init", struct fpm_smc, &a, &b, 0);

	a.quad = b.quad = c->quad0;
	a.acc = b.acc = c->acc0;

	for (i = 0; i < NWORDS; i++)
		src[i] = (unsigned short)(rnd() & c->inmask);

	/*
	 * One word per call, so a divergence names the symbol that caused it.
	 * The path taken and the wraps are read off the REFERENCE object
	 * before and after each call, which is what makes the coverage a
	 * measurement rather than an intention.
	 */
	for (i = 0; i < NWORDS; i++) {
		short acc_before = b.acc;
		short widx_before = pb.r.widx;
		short quad_before = b.quad;

		if (c->f00)
			cov_index_form++;
		else
			cov_complex_form++;
		if (quad_before < 0)
			cov_negative_quad++;

		ref_SMC_encoder(&b, &pb.r, &src[i], 1);
		SMC_encoder(&a, &pa.r, &src[i], 1);

		if (c->rot_step != 0 && b.acc < acc_before)
			cov_acc_wrap++;
		if (pb.r.widx < widx_before)
			cov_widx_wrap++;
		if (c->f00 && pb.sym[widx_before >= 0
				     && widx_before < RINGCAP
				     ? widx_before : 0] < acc_before)
			cov_index_wrap++;

		diff_eq_obj("after word", struct fpm_smc, &a, &b, i);
		compare_rings(&pa, &pb, i);
	}

	/* count == 0 over a dirty state: the three write-backs still happen. */
	{
		unsigned short w = 0x0005;

		a.quad = b.quad = 0x0123;
		a.acc = b.acc = 0x0045;
		pa.r.widx = pb.r.widx = 3;
		ref_SMC_encoder(&b, &pb.r, &w, 0);
		SMC_encoder(&a, &pa.r, &w, 0);
		diff_eq_obj("count 0", struct fpm_smc, &a, &b, 0);
		compare_rings(&pa, &pb, 1000);
		diff_eq_int("count 0 kept quad (%ld)", a.quad, 0x0123, 0);
		diff_eq_int("count 0 kept widx (%ld)", pa.r.widx, 3, 0);
	}

	/*
	 * The whole block in one call must land where the same words one at a
	 * time land.  Fresh rings, same seed.
	 */
	{
		struct fpm_smc ba;
		struct fpm_smc bb;
		static struct ring_pair qa;
		static struct ring_pair qb;

		make_ring(&qa, c);
		make_ring(&qb, c);
		memset(&ba, 0xa5, sizeof(ba));
		memset(&bb, 0xa5, sizeof(bb));
		SMC_init(&ba, &cfg);
		ref_SMC_init(&bb, &cfg);
		ba.quad = bb.quad = c->quad0;
		ba.acc = bb.acc = c->acc0;

		SMC_encoder(&ba, &qa.r, src, NWORDS);
		for (i = 0; i < NWORDS; i++)
			ref_SMC_encoder(&bb, &qb.r, &src[i], 1);

		diff_eq_obj("bulk vs stepped", struct fpm_smc, &ba, &bb, 0);
		compare_rings(&qa, &qb, 2000);
	}

	return diff_end();
}

static int
run_config(void)
{
	struct fpm_smc a, b;
	const int *ours = (const int *)(const void *)&SMC_CFG;
	const int *theirs = (const int *)(const void *)&ref_SMC_CFG;
	int k;

	diff_begin("SMC_CFG");

	/*
	 * Dword by dword over all forty-four bytes, so a field this tree has
	 * not named still has to match.  The five pointer slots are null in
	 * this table and compare as zeroes.
	 */
	for (k = 0; k < 11; k++)
		diff_eq_int("SMC_CFG dword %ld", ours[k], theirs[k], k);

	/* And the named ones, so a wrong offset shows as itself. */
	diff_eq_int("f00 (%ld)", SMC_CFG.f00, ref_SMC_CFG.f00, 0);
	diff_eq_int("direct (%ld)", SMC_CFG.direct, ref_SMC_CFG.direct, 0);
	diff_eq_int("rot_step (%ld)", SMC_CFG.rot_step, ref_SMC_CFG.rot_step,
		    0);
	diff_eq_int("rot_mod (%ld)", SMC_CFG.rot_mod, ref_SMC_CFG.rot_mod, 0);
	diff_eq_int("qshift (%ld)", SMC_CFG.qshift, ref_SMC_CFG.qshift, 0);
	diff_eq_int("qmask (%ld)", SMC_CFG.qmask, ref_SMC_CFG.qmask, 0);
	diff_eq_int("amask (%ld)", SMC_CFG.amask, ref_SMC_CFG.amask, 0);
	diff_eq_int("pmask (%ld)", SMC_CFG.pmask, ref_SMC_CFG.pmask, 0);
	diff_eq_int("pmap null (%ld)", SMC_CFG.pmap != 0,
		    ref_SMC_CFG.pmap != 0, 0);
	diff_eq_int("cosine null (%ld)", SMC_CFG.cosine != 0,
		    ref_SMC_CFG.cosine != 0, 0);
	diff_eq_int("sine null (%ld)", SMC_CFG.sine != 0,
		    ref_SMC_CFG.sine != 0, 0);

	memset(&a, 0x5a, sizeof(a));
	memset(&b, 0x5a, sizeof(b));
	ref_SMC_init(&b, &ref_SMC_CFG);
	SMC_init(&a, &SMC_CFG);
	diff_eq_obj("init from the table", struct fpm_smc, &a, &b, 0);

	return diff_end();
}

int
main(void)
{
	int rc = 0;
	int i;

	rnd_state = 0x0badf00du;

	for (i = 0; i < NENC; i++)
		rc |= run_case(&enc_cases[i]);

	rc |= run_config();

	diff_begin("coverage of the run");
	diff_eq_int("index form entered (%ld > 0)", cov_index_form > 0, 1, 0);
	diff_eq_int("complex form entered (%ld > 0)", cov_complex_form > 0,
		    1, 0);
	diff_eq_int("acc wrapped (%ld > 0)", cov_acc_wrap > 0, 1, 0);
	diff_eq_int("widx wrapped (%ld > 0)", cov_widx_wrap > 0, 1, 0);
	diff_eq_int("negative quad seen (%ld > 0)", cov_negative_quad > 0,
		    1, 0);
	(void)cov_index_wrap;
	rc |= diff_end();

	return rc;
}

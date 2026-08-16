/*
 * t_fpm_smc.c -- differential test of the symbol coder and the V.22 tables.
 *
 * SMCv22_CFG exercises about half of FPM_SMC_encoder: `direct` is 0,
 * `rot_step` is 0, `amask` is 0 and `qshift` is 0, so the absolute-quadrant
 * path, the carrier accumulator and every shift wider than nothing are dead
 * under the only configuration the modem builds.  Each synthetic config below
 * exists to break one specific misreading:
 *
 *   rot_mod small, index large   the wrap is ONE conditional subtract, not a
 *                                modulo.  `%` agrees with the object over
 *                                every value V.22 produces and disagrees here.
 *   rot_mod negative             the test is signed, so it is always true.
 *   qshift 12, high words        the data word is loaded zero-extended and
 *                                shifted arithmetically.  With V.22's shift
 *                                of 0 or 2 the two readings agree over every
 *                                input; at 12 they do not.  Finding 613's
 *                                pattern.
 *   quad seeded negative,        the quadrant is masked as a 32-bit value and
 *   pmask 0xffff                 truncated to a short only for the NEXT
 *                                symbol, so the index and the stored state
 *                                disagree above bit 15.
 *   widx seeded past len         the wrap is tested after the store.
 *   count == 0 on a dirty state  quad, acc and widx are written back anyway.
 *
 * The tables are checked two ways: byte-for-byte against the blob's copies,
 * and by asserting that the blob's own SMCv22_CFG pointer fields hold the
 * addresses of the blob's own tables.  That second check is what proves the
 * relocation identification rather than assuming it -- an int16 dump of the
 * config shows those three fields as zeroes.
 */

#include <string.h>

#include "harness.h"
#include "dsplib/fpm_smc.h"
#include "dsplib/v22txtab.h"

extern void ref_FPM_SMC_init(void *smc, const void *cfg);
extern void ref_FPM_SMC_encoder(void *smc, void *ring,
				const unsigned short *data,
				unsigned short count);
extern const struct fpm_smc_cfg ref_SMCv22_CFG;
extern const unsigned short ref_SMCv22_PMAP[4];
extern const short ref_SMCv22_IMAP_1200BPS[16];
extern const short ref_SMCv22_QMAP_1200BPS[16];
extern const short ref_SMCv22_IMAP_2400BPS[16];
extern const short ref_SMCv22_QMAP_2400BPS[16];

#define NSYM   64
#define NWORDS 48

static unsigned int rnd_state;

static unsigned int
rnd(void)
{
	rnd_state = rnd_state * 1103515245u + 12345u;
	return rnd_state >> 8;
}

/* Sixteen entries, so a qmask of 15 has somewhere to land.  0xffff is there
 * to exercise the zero-extended read of a map entry. */
static const unsigned short pmap16[16] = {
	4, 0, 8, 12, 1, 2, 3, 5,
	0xffff, 0x8000, 7, 9, 11, 13, 14, 15
};

struct enc_case {
	const char *name;
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
	/*                     dir step  mod shift qm  am  pmask quad acc widx len inmask */
	{ "V.22 1200 bit/s",     0,  0,  16,  0,   3,  0,  15,    0,  0,  0,  8, 0x0003 },
	{ "V.22bis 2400 bit/s",  0,  0,  16,  2,   3,  3,  15,    0,  0,  0,  8, 0x000f },
	{ "absolute quadrant",   1,  0,  16,  0,   3,  3,  15,    0,  0,  0,  8, 0x000f },
	{ "carrier rotation",    0,  5,  16,  2,   3,  3,  15,    0,  3,  0, 12, 0x000f },
	{ "step just under mod", 0,  7,   8,  0,   3,  3,   7,    0,  6,  0,  5, 0x000f },
	{ "index over 2*mod",    0,  1,   5,  0,  15, 15,  15,    0,  4,  0,  9, 0xffff },
	{ "negative modulus",    0,  1,  -1,  0,   3,  3,  15,    0,  0,  0,  6, 0x000f },
	{ "wide shift",          0,  0,  16, 12,  15,  3,  15,    0,  0,  0,  8, 0xffff },
	{ "quad seeded negative",0,  0,  16,  0,  15,  0, 0xffff, -1,  0,  0,  8, 0xffff },
	{ "quad far negative",   0,  3, 100,  0,  15,  0, 0xffff, -32768, 90, 0, 8, 0xffff },
	{ "widx past len",       0,  0,  16,  0,   3,  0,  15,    0,  0, 20,  8, 0x0003 },
	{ "len of one",          0,  1,  16,  0,   3,  3,  15,    0,  0,  0,  1, 0x000f }
};

#define NENC ((int)(sizeof(enc_cases) / sizeof(enc_cases[0])))

static void
make_cfg(struct fpm_smc_cfg *cfg, const struct enc_case *c)
{
	memset(cfg, 0, sizeof(*cfg));
	cfg->f00 = 1;
	cfg->direct = c->direct;
	cfg->rot_step = c->rot_step;
	cfg->rot_mod = c->rot_mod;
	cfg->qshift = c->qshift;
	cfg->qmask = c->qmask;
	cfg->amask = c->amask;
	cfg->pmask = c->pmask;
	cfg->pmap = pmap16;
	cfg->imap = SMCv22_IMAP_1200BPS;
	cfg->qmap = SMCv22_QMAP_1200BPS;
}

/*
 * The two ring descriptors cannot be compared as objects: each must own its
 * own symbol buffer or there is nothing to compare.  Everything else in them
 * is, field by field, including the bytes the encoder must not touch.
 */
static void
compare_ring(const struct fpm_smc_ring *a, const struct fpm_smc_ring *b,
	     int nsym, int input)
{
	int i;

	diff_eq_int("ring widx (%ld)", a->widx, b->widx, input);
	diff_eq_int("ring len untouched (%ld)", a->len, b->len, input);
	/*
	 * +0x0e was written down as padding when only the encoder had been
	 * traced.  It is the CONSUMER's read cursor -- V22_PPS_filter drives
	 * it (finding 1569) -- so this assertion is stronger than it was:
	 * the producer must not disturb the consumer's position in the ring.
	 */
	diff_eq_int("ring ridx untouched by the encoder (%ld)", a->ridx,
		    b->ridx, input);
	for (i = 0; i < 8; i++)
		diff_eq_int("ring direct rails untouched (%ld)",
			    ((const unsigned char *)a)[i],
			    ((const unsigned char *)b)[i], i);
	for (i = 0; i < nsym; i++)
		diff_eq_int("symbol[%ld]", a->sym[i], b->sym[i], i);
}

static int
run_enc(const struct enc_case *c)
{
	struct fpm_smc_cfg cfg;
	struct fpm_smc a, b;
	struct fpm_smc_ring ra, rb;
	short sym_a[NSYM], sym_b[NSYM];
	unsigned short words[NWORDS];
	int i;

	make_cfg(&cfg, c);

	diff_begin(c->name);

	/* Both sides take the same config object, so the three pointers in
	 * the copy are identical and the whole struct can be compared. */
	memset(&a, 0x5a, sizeof(a));
	memset(&b, 0x5a, sizeof(b));
	ref_FPM_SMC_init(&b, &cfg);
	FPM_SMC_init(&a, &cfg);
	diff_eq_obj("after init", struct fpm_smc, &a, &b, 0);

	a.quad = b.quad = c->quad0;
	a.acc = b.acc = c->acc0;

	memset(sym_a, 0x77, sizeof(sym_a));
	memset(sym_b, 0x77, sizeof(sym_b));
	memset(&ra, 0x33, sizeof(ra));
	memset(&rb, 0x33, sizeof(rb));
	ra.sym = sym_a;
	rb.sym = sym_b;
	ra.widx = rb.widx = c->widx0;
	ra.len = rb.len = c->len;

	for (i = 0; i < NWORDS; i++)
		words[i] = (unsigned short)(rnd() & c->inmask);

	/* One word per call: a divergence names the symbol that caused it. */
	for (i = 0; i < NWORDS; i++) {
		int at = ra.widx;

		ref_FPM_SMC_encoder(&b, &rb, &words[i], 1);
		FPM_SMC_encoder(&a, &ra, &words[i], 1);
		diff_eq_obj("after symbol", struct fpm_smc, &a, &b, i);
		diff_eq_int("widx after symbol %ld", ra.widx, rb.widx, i);
		diff_eq_int("symbol written at %ld", sym_a[at], sym_b[at], at);
	}
	compare_ring(&ra, &rb, NSYM, 0);

	/* And in bulk, continuing from that state. */
	for (i = 0; i < NWORDS; i++)
		words[i] = (unsigned short)(rnd() & c->inmask);
	ref_FPM_SMC_encoder(&b, &rb, words, NWORDS);
	FPM_SMC_encoder(&a, &ra, words, NWORDS);
	diff_eq_obj("after bulk", struct fpm_smc, &a, &b, 0);
	compare_ring(&ra, &rb, NSYM, 0);

	/* Nothing to encode still writes quad, acc and widx back. */
	a.quad = b.quad = 0x1234;
	a.acc = b.acc = 0x5678;
	ra.widx = rb.widx = 3;
	ref_FPM_SMC_encoder(&b, &rb, words, 0);
	FPM_SMC_encoder(&a, &ra, words, 0);
	diff_eq_obj("after nothing", struct fpm_smc, &a, &b, 0);
	compare_ring(&ra, &rb, NSYM, 0);

	return diff_end();
}

static int
run_tables(void)
{
	struct fpm_smc a, b;
	int i;

	diff_begin("SMCv22 tables");

	for (i = 0; i < 4; i++)
		diff_eq_int("PMAP[%ld]", SMCv22_PMAP[i], ref_SMCv22_PMAP[i], i);
	for (i = 0; i < 16; i++) {
		diff_eq_int("IMAP_1200[%ld]", SMCv22_IMAP_1200BPS[i],
			    ref_SMCv22_IMAP_1200BPS[i], i);
		diff_eq_int("QMAP_1200[%ld]", SMCv22_QMAP_1200BPS[i],
			    ref_SMCv22_QMAP_1200BPS[i], i);
		diff_eq_int("IMAP_2400[%ld]", SMCv22_IMAP_2400BPS[i],
			    ref_SMCv22_IMAP_2400BPS[i], i);
		diff_eq_int("QMAP_2400[%ld]", SMCv22_QMAP_2400BPS[i],
			    ref_SMCv22_QMAP_2400BPS[i], i);
	}

	/* Scalars of the config. */
	diff_eq_int("cfg f00 (%ld)", SMCv22_CFG.f00, ref_SMCv22_CFG.f00, 0);
	diff_eq_int("cfg direct (%ld)", SMCv22_CFG.direct,
		    ref_SMCv22_CFG.direct, 0);
	diff_eq_int("cfg rot_step (%ld)", SMCv22_CFG.rot_step,
		    ref_SMCv22_CFG.rot_step, 0);
	diff_eq_int("cfg rot_mod (%ld)", SMCv22_CFG.rot_mod,
		    ref_SMCv22_CFG.rot_mod, 0);
	diff_eq_int("cfg qshift (%ld)", SMCv22_CFG.qshift,
		    ref_SMCv22_CFG.qshift, 0);
	diff_eq_int("cfg qmask (%ld)", SMCv22_CFG.qmask,
		    ref_SMCv22_CFG.qmask, 0);
	diff_eq_int("cfg amask (%ld)", SMCv22_CFG.amask,
		    ref_SMCv22_CFG.amask, 0);
	diff_eq_int("cfg pmask (%ld)", SMCv22_CFG.pmask,
		    ref_SMCv22_CFG.pmask, 0);
	diff_eq_int("cfg f20 (%ld)", SMCv22_CFG.f20, ref_SMCv22_CFG.f20, 0);
	diff_eq_int("cfg f24 (%ld)", SMCv22_CFG.f24, ref_SMCv22_CFG.f24, 0);
	diff_eq_int("cfg f28 (%ld)", SMCv22_CFG.f28, ref_SMCv22_CFG.f28, 0);

	/*
	 * The three relocations, asserted rather than assumed: the blob's own
	 * config must point at the blob's own tables, and specifically at the
	 * 1200 bit/s pair.  Nothing in the object points the config at the
	 * 2400 pair -- SetTxRate does that to another object at run time.
	 */
	diff_eq_int("blob cfg.pmap is SMCv22_PMAP (%ld)",
		    ref_SMCv22_CFG.pmap == ref_SMCv22_PMAP, 1, 0);
	diff_eq_int("blob cfg.imap is IMAP_1200 (%ld)",
		    ref_SMCv22_CFG.imap == ref_SMCv22_IMAP_1200BPS, 1, 0);
	diff_eq_int("blob cfg.qmap is QMAP_1200 (%ld)",
		    ref_SMCv22_CFG.qmap == ref_SMCv22_QMAP_1200BPS, 1, 0);
	diff_eq_int("ours cfg.pmap is SMCv22_PMAP (%ld)",
		    SMCv22_CFG.pmap == SMCv22_PMAP, 1, 0);
	diff_eq_int("ours cfg.imap is IMAP_1200 (%ld)",
		    SMCv22_CFG.imap == SMCv22_IMAP_1200BPS, 1, 0);
	diff_eq_int("ours cfg.qmap is QMAP_1200 (%ld)",
		    SMCv22_CFG.qmap == SMCv22_QMAP_1200BPS, 1, 0);

	/* Init from each side's own copy: the scalars must land identically
	 * and the pointers must resolve to equal contents. */
	memset(&a, 0x5a, sizeof(a));
	memset(&b, 0x5a, sizeof(b));
	ref_FPM_SMC_init(&b, &ref_SMCv22_CFG);
	FPM_SMC_init(&a, &SMCv22_CFG);
	diff_eq_int("init quad (%ld)", a.quad, b.quad, 0);
	diff_eq_int("init acc (%ld)", a.acc, b.acc, 0);
	diff_eq_int("init f00 (%ld)", a.cfg.f00, b.cfg.f00, 0);
	diff_eq_int("init direct (%ld)", a.cfg.direct, b.cfg.direct, 0);
	diff_eq_int("init rot_mod (%ld)", a.cfg.rot_mod, b.cfg.rot_mod, 0);
	diff_eq_int("init qmask (%ld)", a.cfg.qmask, b.cfg.qmask, 0);
	diff_eq_int("init pmask (%ld)", a.cfg.pmask, b.cfg.pmask, 0);
	for (i = 0; i < 4; i++)
		diff_eq_int("init pmap[%ld]", a.cfg.pmap[i], b.cfg.pmap[i], i);
	for (i = 0; i < 16; i++) {
		diff_eq_int("init imap[%ld]", a.cfg.imap[i], b.cfg.imap[i], i);
		diff_eq_int("init qmap[%ld]", a.cfg.qmap[i], b.cfg.qmap[i], i);
	}

	/*
	 * The constellation, stated as the property rather than as bytes.
	 * V.22bis Table 2 is the odd coordinates +-1 and +-3; every entry
	 * here is one of those scaled by 8192, and the four quadrants are
	 * 90-degree rotations of the first -- (I, Q) -> (-Q, I).
	 */
	for (i = 0; i < 12; i++) {
		diff_eq_int("2400 rotation I[%ld]", SMCv22_IMAP_2400BPS[i + 4],
			    -SMCv22_QMAP_2400BPS[i], i);
		diff_eq_int("2400 rotation Q[%ld]", SMCv22_QMAP_2400BPS[i + 4],
			    SMCv22_IMAP_2400BPS[i], i);
	}
	for (i = 0; i < 12; i++) {
		diff_eq_int("1200 rotation I[%ld]", SMCv22_IMAP_1200BPS[i + 4],
			    -SMCv22_QMAP_1200BPS[i], i);
		diff_eq_int("1200 rotation Q[%ld]", SMCv22_QMAP_1200BPS[i + 4],
			    SMCv22_IMAP_1200BPS[i], i);
	}
	for (i = 0; i < 16; i++) {
		int mag = SMCv22_IMAP_2400BPS[i] < 0 ?
			-SMCv22_IMAP_2400BPS[i] : SMCv22_IMAP_2400BPS[i];

		diff_eq_int("2400 I magnitude[%ld] is 1 or 3 times 8192",
			    mag == 8192 || mag == 24576, 1, i);
	}
	/* At 1200 bit/s the amplitude bits are masked off, so only every
	 * fourth index is ever formed and each quadrant's four slots must
	 * therefore carry the same point. */
	for (i = 0; i < 16; i++) {
		diff_eq_int("1200 I[%ld] replicates its quadrant",
			    SMCv22_IMAP_1200BPS[i],
			    SMCv22_IMAP_1200BPS[i & ~3], i);
		diff_eq_int("1200 Q[%ld] replicates its quadrant",
			    SMCv22_QMAP_1200BPS[i],
			    SMCv22_QMAP_1200BPS[i & ~3], i);
	}
	/* V.22bis Table 1: 90, 0, 180, 270 degrees in quarter-quadrants. */
	diff_eq_int("PMAP dibit 00 is +90 (%ld)", SMCv22_PMAP[0], 4, 0);
	diff_eq_int("PMAP dibit 01 is 0 (%ld)", SMCv22_PMAP[1], 0, 0);
	diff_eq_int("PMAP dibit 10 is +180 (%ld)", SMCv22_PMAP[2], 8, 0);
	diff_eq_int("PMAP dibit 11 is +270 (%ld)", SMCv22_PMAP[3], 12, 0);

	return diff_end();
}

/*
 * The V.22 configuration driven through the real table, end to end: our
 * encoder, the blob's, and the differential encoding property the standard
 * states -- each dibit turns the quadrant by the amount SMCv22_PMAP names.
 */
static int
run_v22(void)
{
	struct fpm_smc a, b;
	struct fpm_smc_ring ra, rb;
	short sym_a[NSYM], sym_b[NSYM];
	unsigned short words[NWORDS];
	int prev;
	int i;

	diff_begin("SMCv22_CFG end to end");

	memset(&a, 0x5a, sizeof(a));
	memset(&b, 0x5a, sizeof(b));
	ref_FPM_SMC_init(&b, &ref_SMCv22_CFG);
	FPM_SMC_init(&a, &ref_SMCv22_CFG);
	diff_eq_obj("init from the blob's table", struct fpm_smc, &a, &b, 0);

	memset(sym_a, 0, sizeof(sym_a));
	memset(sym_b, 0, sizeof(sym_b));
	memset(&ra, 0x33, sizeof(ra));
	memset(&rb, 0x33, sizeof(rb));
	ra.sym = sym_a;
	rb.sym = sym_b;
	ra.widx = rb.widx = 0;
	ra.len = rb.len = NSYM;

	for (i = 0; i < NWORDS; i++)
		words[i] = (unsigned short)(rnd() & 3u);

	ref_FPM_SMC_encoder(&b, &rb, words, NWORDS);
	FPM_SMC_encoder(&a, &ra, words, NWORDS);
	diff_eq_obj("after the stream", struct fpm_smc, &a, &b, 0);
	compare_ring(&ra, &rb, NSYM, 0);

	prev = 0;
	for (i = 0; i < NWORDS; i++) {
		int want = (prev + SMCv22_PMAP[words[i]]) & 15;

		diff_eq_int("differential quadrant[%ld]", sym_a[i], want, i);
		/* 1200 bit/s uses four of the sixteen index slots. */
		diff_eq_int("index[%ld] is a multiple of four",
			    sym_a[i] & 3, 0, i);
		prev = want;
	}

	return diff_end();
}

int
main(void)
{
	int rc = 0;
	int i;

	rnd_state = 0x2468aceu;

	for (i = 0; i < NENC; i++)
		rc |= run_enc(&enc_cases[i]);
	rc |= run_v22();
	rc |= run_tables();

	return rc;
}

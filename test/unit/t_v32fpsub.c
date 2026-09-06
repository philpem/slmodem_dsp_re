/*
 * t_v32fpsub.c -- differential test of V.32's four sub-object operations.
 *
 * The rest of src/pump/v32/v32fpctl.c, plus src/dsp/fpm_fse.c's `FSE_getdiag`:
 * the four functions that CONSTRUCT or DESTROY something rather than only
 * storing into it, and so need a fixture with real buffers behind it.
 *
 *   FSE_getdiag / V32FP_GetDiagnostics   the equaliser's scatter log
 *   SetAdaptEcV32                        four modes, one of which re-inits
 *   SetToneDetect                        retunes through FPM_TONE_create
 *   V32FP_delete                         fourteen frees in one order
 *
 * ---------------------------------------------------------------------------
 * `V32FP_delete` IS TESTED THROUGH THE ALLOCATOR, NOT THROUGH MEMORY
 *
 * Every block it releases is gone by the time it returns, so there is nothing
 * left to compare.  What there IS is the harness allocator's live set: each of
 * the fourteen targets gets its OWN allocation, and so does a DECOY at every
 * neighbouring offset the function might have reached by mistake.  After the
 * call, "which of these twenty-two are still live" is a bit pattern, and the
 * two sides' patterns are compared.  A free of the wrong field then shows as
 * a live target and a dead decoy at once.
 *
 * The counters go with it -- `frees`, `free_null`, `bad_free` -- because a
 * function that freed the right things in the wrong ORDER, or freed one twice,
 * leaves the same live set and a different tally.  `bad_free` must be zero on
 * both sides and is checked rather than assumed.
 *
 * ---------------------------------------------------------------------------
 * WHAT THE `FSE_getdiag` SWEEP IS FOR
 *
 * Its two arms are deliberately asymmetric in the object -- one has a capacity
 * guard and the other has none, and one empties the log after the copy and the
 * other before it.  So the sweep drives `diag_n` at, just below and just above
 * the guard; `max` above and below the waiting count; and a NEGATIVE count,
 * which neither arm guards against and which therefore comes straight back out
 * as the return value.
 *
 * THE TWO EMPTYING ORDERS ARE NOT SEPARATED BY ANYTHING HERE, and saying so is
 * the point: they differ only if `out` overlaps the block, which no caller in
 * the object arranges.  Finding F8163's shape -- reaching a branch is not
 * telling it from its alternative.
 */

#include <stddef.h>
#include <stdio.h>
#include <string.h>

#include "harness.h"
#include "dsplib/v32fpctl.h"
#include "dsplib/fpm_ecc.h"
#include "dsplib/fpm_fse.h"
#include "dsplib/fpm_mtd.h"
#include "dsplib/fpm_tone.h"
#include "dsplib/sysdep.h"

/* --------------------------------------------------------------------- */

extern int ref_FSE_getdiag(struct fpm_fse *state, int which,
			   struct fpm_fse_point *out, int max);
extern int ref_V32FP_GetDiagnostics(void *modem, int which,
				    struct fpm_fse_point *out, int max);
extern void ref_SetAdaptEcV32(void *modem, unsigned short mode);
extern void ref_SetToneDetect(void *modem, short hz);
extern void ref_V32FP_delete(void *modem);

/* --------------------------------------------------------------------- */

#define OBJ_SIZE	0x80
#define HDX_SIZE	0x100
#define FP_SIZE		0x5100

#define DIAGOUT		2100

#define ECC_NEAR	8
#define ECC_FAR		4
#define ECC_LAG		16
#define ECC_FILL	0x10
#define ECC_LINE	160
#define ECC_COEF	64

#define TONE_LEN	16
#define TONE_EXTRA	4

static unsigned rng_state;

static void
rng_seed(unsigned s)
{
	rng_state = s ? s : 1u;
}

static unsigned
rng_next(void)
{
	rng_state ^= rng_state << 13;
	rng_state ^= rng_state >> 17;
	rng_state ^= rng_state << 5;
	return rng_state;
}

static void
fill(void *p, int n, unsigned seed)
{
	unsigned char *b = (unsigned char *)p;
	int i;

	rng_seed(seed);
	for (i = 0; i < n; i++)
		b[i] = (unsigned char)rng_next();
}

static void
put_ptr(void *base, int off, void *v)
{
	*(void **)(void *)((unsigned char *)base + off) = v;
}

static void
put_s16(void *base, int off, short v)
{
	*(short *)(void *)((unsigned char *)base + off) = v;
}

static void
put_int(void *base, int off, int v)
{
	*(int *)(void *)((unsigned char *)base + off) = v;
}

struct skip {
	int off;
	int len;
};

static int
first_diff(const void *a, const void *b, int len, const struct skip *sk,
	   int nsk)
{
	const unsigned char *x = (const unsigned char *)a;
	const unsigned char *y = (const unsigned char *)b;
	int i, j;

	for (i = 0; i < len; i++) {
		if (x[i] == y[i])
			continue;
		for (j = 0; j < nsk; j++)
			if (i >= sk[j].off && i < sk[j].off + sk[j].len)
				break;
		if (j == nsk)
			return i;
	}
	return -1;
}

static void
cmp_one(const char *what, const char *blk, const void *a, const void *b,
	int len, const struct skip *sk, int nsk, long tag)
{
	char fmt[192];

	sprintf(fmt, "%s: %s differs at byte (tag %%ld)", what, blk);
	diff_eq_int(fmt, first_diff(a, b, len, sk, nsk), -1, tag);
}

/* --------------------------------------------------------------------- */
/* Non-vacuity counters.                                                  */

static long gd_arm[3];			/* which == 0, 1, other            */
static long gd_capacity_hit, gd_capped, gd_negative, gd_copied;
static long ec_arm[5];
static long ec_line_cleared, ec_mu_divided;
static long tone_retuned;
static long del_runs, del_decoys_survived;

/* --------------------------------------------------------------------- */
/* 1.  FSE_getdiag, direct and through V32FP_GetDiagnostics.              */

static struct fpm_fse fa, fb;
static struct fpm_fse_point outa[DIAGOUT], outb[DIAGOUT];

static void
run_getdiag(void)
{
	static const int whichv[] = { -1, 0, 1, 2 };
	static const int n1v[] = { -3, 0, 1, 5, 478, 479, 480, 481 };
	static const int n2v[] = { -3, 0, 1, 5, 1999, 2000 };
	static const int maxv[] = { 0, 1, 4, 600, 2048 };
	unsigned w, n, m;
	unsigned seed = 0x33aa;

	for (w = 0; w < sizeof(whichv) / sizeof(whichv[0]); w++)
	for (n = 0; n < 8; n++)
	for (m = 0; m < sizeof(maxv) / sizeof(maxv[0]); m++) {
		int ra, rb;

		fill(&fa, (int)sizeof(fa), seed);
		fill(&fb, (int)sizeof(fb), seed);
		fill(outa, (int)sizeof(outa), seed ^ 0x5a5au);
		fill(outb, (int)sizeof(outb), seed ^ 0x5a5au);
		seed = seed * 1103515245u + 12345u;

		fa.diag_n = n1v[n];
		fb.diag_n = n1v[n];
		fa.diag2_n = n2v[n % (sizeof(n2v) / sizeof(n2v[0]))];
		fb.diag2_n = fa.diag2_n;

		ra = FSE_getdiag(&fa, whichv[w], outa, maxv[m]);
		rb = ref_FSE_getdiag(&fb, whichv[w], outb, maxv[m]);

		diff_eq_int("FSE_getdiag returned (which/max tag %ld)",
			    (long)ra, (long)rb,
			    (long)(whichv[w] * 100000 + maxv[m]));
		cmp_one("FSE_getdiag", "the block", &fa, &fb,
			(int)sizeof(fa), 0, 0, (long)n1v[n]);
		cmp_one("FSE_getdiag", "the output", outa, outb,
			(int)sizeof(outa), 0, 0, (long)n1v[n]);

		if (whichv[w] == 0) {
			gd_arm[0]++;
			if (n1v[n] > FPM_FSE_DIAG - 1)
				gd_capacity_hit++;
			if (n1v[n] > maxv[m] && n1v[n] <= FPM_FSE_DIAG - 1)
				gd_capped++;
		} else if (whichv[w] == 1) {
			gd_arm[1]++;
		} else {
			gd_arm[2]++;
		}
		if (ra < 0)
			gd_negative++;
		if (ra > 0)
			gd_copied++;
	}
}

/* --------------------------------------------------------------------- */
/* 2.  The wrapper, over a datapump block with the equaliser at fp + 0x204. */

struct wrap {
	unsigned char	obj[OBJ_SIZE];
	unsigned char	fp[FP_SIZE];
	double		align;
};

static struct wrap wa, wb;

static void
run_getdiag_wrapper(void)
{
	static const int whichv[] = { 0, 1, 7 };
	unsigned w;
	int trial;

	for (w = 0; w < sizeof(whichv) / sizeof(whichv[0]); w++)
	for (trial = 0; trial < 3; trial++) {
		unsigned seed = 0x6100u + w * 17u + (unsigned)trial;
		int ra, rb;

		fill(&wa, offsetof(struct wrap, align), seed);
		fill(&wb, offsetof(struct wrap, align), seed);
		fill(outa, (int)sizeof(outa), seed ^ 0x1111u);
		fill(outb, (int)sizeof(outb), seed ^ 0x1111u);

		put_ptr(wa.obj, V32_OBJ_FP, wa.fp);
		put_ptr(wb.obj, V32_OBJ_FP, wb.fp);
		put_int(wa.fp, V32FP_FSE + 0x4e0c, 3 + trial);
		put_int(wb.fp, V32FP_FSE + 0x4e0c, 3 + trial);
		put_int(wa.fp, V32FP_FSE + 0x4e14, 11 + trial);
		put_int(wb.fp, V32FP_FSE + 0x4e14, 11 + trial);

		ra = V32FP_GetDiagnostics(wa.obj, whichv[w], outa, 32);
		rb = ref_V32FP_GetDiagnostics(wb.obj, whichv[w], outb, 32);

		diff_eq_int("V32FP_GetDiagnostics returned (which %ld)",
			    (long)ra, (long)rb, (long)whichv[w]);
		{
			static const struct skip sk[] = { { V32_OBJ_FP, 4 } };

			cmp_one("V32FP_GetDiagnostics", "obj", wa.obj, wb.obj,
				OBJ_SIZE, sk, 1, (long)whichv[w]);
		}
		cmp_one("V32FP_GetDiagnostics", "fp", wa.fp, wb.fp, FP_SIZE, 0,
			0, (long)whichv[w]);
		cmp_one("V32FP_GetDiagnostics", "out", outa, outb,
			(int)sizeof(outa), 0, 0, (long)whichv[w]);
	}
}

/* --------------------------------------------------------------------- */
/* 3.  SetAdaptEcV32.                                                     */

struct ecfix {
	unsigned char	obj[OBJ_SIZE];
	unsigned char	hdx[HDX_SIZE];
	short		near_i[ECC_NEAR];
	short		near_q[ECC_NEAR];
	short		far_i[ECC_FAR];
	short		far_q[ECC_FAR];
	short		line[ECC_LINE];
	short		coef[3][ECC_COEF];
	unsigned char	fp[FP_SIZE];
	double		align;
};

static struct ecfix ea, eb;

static void
ecfix_build(struct ecfix *f, unsigned seed, short near_delay, short line_len)
{
	unsigned char *e;

	fill(f, (int)offsetof(struct ecfix, align), seed);

	put_ptr(f->obj, V32_OBJ_HDX, f->hdx);
	put_ptr(f->obj, V32_OBJ_FP, f->fp);
	put_s16(f->obj, V32_OBJ_EC_NEAR_DELAY, near_delay);

	e = f->fp + V32FP_ECC;
	put_s16(e, 0x00, ECC_LAG);		/* cfg.far_lag              */
	put_s16(e, 0x02, ECC_NEAR);		/* cfg.near_taps            */
	put_s16(e, 0x04, ECC_FAR);		/* cfg.far_taps             */
	put_s16(e, 0x06, 0);
	put_ptr(e, 0x08, 0);			/* cfg.imap, unread by init */
	put_ptr(e, 0x0c, 0);			/* cfg.qmap                 */
	put_s16(e, 0x10, ECC_FILL);		/* cfg.fill                 */
	put_s16(e, 0x12, 0);
	put_ptr(e, 0x14, 0);			/* cfg.aux                  */
	put_s16(e, 0x34, line_len);		/* line_len                 */
	put_ptr(e, 0x38, f->line);
	put_ptr(e, 0x40, f->near_i);
	put_ptr(e, 0x44, f->near_q);
	put_ptr(e, 0x4c, f->far_i);
	put_ptr(e, 0x50, f->far_q);
	put_ptr(e, 0x54, f->coef[0]);
	put_ptr(e, 0x58, f->coef[1]);
	put_ptr(e, 0x5c, f->coef[2]);
	put_s16(e, 0x66, 0);			/* far_delay, set by mode 0 */
}

static const struct skip ec_fp_skip[] = {
	{ V32FP_ECC + 0x38, 4 },
	{ V32FP_ECC + 0x40, 8 },
	{ V32FP_ECC + 0x4c, 8 },
	{ V32FP_ECC + 0x54, 12 }
};

static void
run_adaptec(void)
{
	int mode;
	int trial;

	for (mode = -1; mode <= 5; mode++)
	for (trial = 0; trial < 3; trial++) {
		unsigned seed = 0x9000u + (unsigned)(mode + 1) * 37u
				+ (unsigned)trial;
		short near_delay = (short)(4 + trial * 4);
		short line_len = (short)(trial == 2 ? 0 : 24 + trial * 40);
		static const struct skip objskip[] = {
			{ V32_OBJ_HDX, 4 }, { V32_OBJ_FP, 4 }
		};

		ecfix_build(&ea, seed, near_delay, line_len);
		ecfix_build(&eb, seed, near_delay, line_len);
		/* A gain the divide-by-five arm can actually move. */
		put_s16(ea.fp + V32FP_ECC, 0x60, (short)(41 + trial * 300));
		put_s16(eb.fp + V32FP_ECC, 0x60, (short)(41 + trial * 300));

		SetAdaptEcV32(ea.obj, (unsigned short)mode);
		ref_SetAdaptEcV32(eb.obj, (unsigned short)mode);

		cmp_one("SetAdaptEcV32", "obj", ea.obj, eb.obj, OBJ_SIZE,
			objskip, 2, (long)mode);
		cmp_one("SetAdaptEcV32", "hdx", ea.hdx, eb.hdx, HDX_SIZE, 0, 0,
			(long)mode);
		cmp_one("SetAdaptEcV32", "fp", ea.fp, eb.fp, FP_SIZE,
			ec_fp_skip,
			(int)(sizeof(ec_fp_skip) / sizeof(ec_fp_skip[0])),
			(long)mode);
		cmp_one("SetAdaptEcV32", "delay line", ea.line, eb.line,
			(int)sizeof(ea.line), 0, 0, (long)mode);
		cmp_one("SetAdaptEcV32", "coefficients", ea.coef, eb.coef,
			(int)sizeof(ea.coef), 0, 0, (long)mode);
		cmp_one("SetAdaptEcV32", "near rails", ea.near_i, eb.near_i,
			(int)(sizeof(ea.near_i) + sizeof(ea.near_q)), 0, 0,
			(long)mode);

		if (mode >= 0 && mode <= 3)
			ec_arm[mode]++;
		else
			ec_arm[4]++;
		if (mode == V32_ADAPTEC_ON && line_len > 0
		    && ea.line[0] == 0)
			ec_line_cleared++;
		if (mode == V32_ADAPTEC_SLOW
		    && *(short *)(void *)(ea.fp + V32FP_ECC + 0x60)
		       != (short)(41 + trial * 300))
			ec_mu_divided++;
	}
}

/* --------------------------------------------------------------------- */
/* 4.  SetToneDetect.                                                     */

static const short tone_src[TONE_LEN] = {
	1000, -900, 800, -700, 600, -500, 400, -300,
	200, -100, 50, -25, 12, -6, 3, -1
};

/*
 * FPM_TONE_create writes through FOUR of the object's buffers even when it did
 * not allocate them -- the kernel and history for `len` entries, five words of
 * `rev_block` and four of `rev_acc`.  All four are supplied here, per fixture.
 */
struct tonefix {
	unsigned char	obj[OBJ_SIZE];
	unsigned char	hdx[HDX_SIZE];
	struct fpm_tone	tone[3];
	short		kern[3][TONE_LEN];
	short		hist[3][TONE_LEN + TONE_EXTRA];
	short		revb[3][8];
	short		reva[3][8];
	double		align;
};

static struct tonefix ta, tb;

static void
tonefix_build(struct tonefix *f, unsigned seed)
{
	int t;

	fill(f, (int)offsetof(struct tonefix, align), seed);

	put_ptr(f->obj, V32_OBJ_HDX, f->hdx);
	for (t = 0; t < 3; t++) {
		f->tone[t].cfg = FPM_TONE_CFG;
		f->tone[t].cfg.freq = (short)(1100 + t * 500);
		/*
		 * The three configurations have to differ in something
		 * `FPM_TONE_create` READS and that the setter does not
		 * overwrite -- otherwise copying the wrong detector's is
		 * invisible, which is exactly how a mutation of it survived
		 * the first run.  `damp` feeds two of the notch coefficients
		 * and `scale` and `ratio` reach the copied block.
		 */
		f->tone[t].cfg.damp = (short)(31000 - t * 2500);
		f->tone[t].cfg.scale = (short)(8000 + t * 700);
		f->tone[t].cfg.ratio = (short)(20000 + t * 1234);
		f->tone[t].cfg.src = tone_src;
		f->tone[t].cfg.len = TONE_LEN;
		f->tone[t].cfg.extra = TONE_EXTRA;
		f->tone[t].kernel = f->kern[t];
		f->tone[t].history = f->hist[t];
		f->tone[t].rev_block = f->revb[t];
		f->tone[t].rev_acc = f->reva[t];
	}
	put_ptr(f->hdx, V32_HDX_TONE0, &f->tone[0]);
	put_ptr(f->hdx, V32_HDX_TONE1, &f->tone[1]);
	put_ptr(f->hdx, V32_HDX_TONE2, &f->tone[2]);
}

static void
run_settone(void)
{
	static const short hz[] = { 0, 400, 1100, 2100, 2225, 3000, -600,
				    32767 };
	unsigned i;

	for (i = 0; i < sizeof(hz) / sizeof(hz[0]); i++) {
		static const struct skip objskip[] = { { V32_OBJ_HDX, 4 } };
		struct skip hdxskip[3];
		int t;

		tonefix_build(&ta, 0xc300u + i * 41u);
		tonefix_build(&tb, 0xc300u + i * 41u);

		SetToneDetect(ta.obj, hz[i]);
		ref_SetToneDetect(tb.obj, hz[i]);

		hdxskip[0].off = V32_HDX_TONE0;
		hdxskip[0].len = 4;
		hdxskip[1].off = V32_HDX_TONE1;
		hdxskip[1].len = 4;
		hdxskip[2].off = V32_HDX_TONE2;
		hdxskip[2].len = 4;

		cmp_one("SetToneDetect", "obj", ta.obj, tb.obj, OBJ_SIZE,
			objskip, 1, (long)hz[i]);
		cmp_one("SetToneDetect", "hdx", ta.hdx, tb.hdx, HDX_SIZE,
			hdxskip, 3, (long)hz[i]);
		cmp_one("SetToneDetect", "kernels", ta.kern, tb.kern,
			(int)sizeof(ta.kern), 0, 0, (long)hz[i]);
		cmp_one("SetToneDetect", "histories", ta.hist, tb.hist,
			(int)sizeof(ta.hist), 0, 0, (long)hz[i]);
		cmp_one("SetToneDetect", "reversal blocks", ta.revb, tb.revb,
			(int)sizeof(ta.revb), 0, 0, (long)hz[i]);
		cmp_one("SetToneDetect", "reversal accumulators", ta.reva,
			tb.reva, (int)sizeof(ta.reva), 0, 0, (long)hz[i]);

		/*
		 * The three tone objects hold one pointer each -- the kernel
		 * -- and it names this fixture's own storage.  Everything else
		 * about them is compared, including the two that must NOT have
		 * moved.
		 */
		for (t = 0; t < 3; t++) {
			struct skip tsk[5];
			char label[32];

			tsk[0].off = (int)offsetof(struct fpm_tone, kernel);
			tsk[1].off = (int)offsetof(struct fpm_tone, history);
			tsk[2].off = (int)offsetof(struct fpm_tone, rev_block);
			tsk[3].off = (int)offsetof(struct fpm_tone, rev_acc);
			/*
			 * +0xfc is the notch's self-pointer, which create sets
			 * to the object's own `iir_coeff` -- a different
			 * address per fixture for ever.  Checked per side just
			 * below rather than only skipped.
			 */
			tsk[4].off = (int)offsetof(struct fpm_tone, iir_self);
			tsk[0].len = tsk[1].len = tsk[2].len = tsk[3].len =
				tsk[4].len = 4;
			sprintf(label, "tone[%d]", t);
			cmp_one("SetToneDetect", label, &ta.tone[t],
				&tb.tone[t], (int)sizeof(ta.tone[t]), tsk, 5,
				(long)hz[i]);
			if (t == 0) {
				diff_eq_int("SetToneDetect: ours re-aimed the "
					    "notch self-pointer (%ld Hz)",
					    ta.tone[0].iir_self
					    == ta.tone[0].iir_coeff, 1,
					    (long)hz[i]);
				diff_eq_int("SetToneDetect: the blob re-aimed "
					    "its own (%ld Hz)",
					    tb.tone[0].iir_self
					    == tb.tone[0].iir_coeff, 1,
					    (long)hz[i]);
			}
		}

		diff_eq_int("SetToneDetect retuned the first detector (%ld Hz)",
			    (long)ta.tone[0].cfg.freq, (long)hz[i],
			    (long)hz[i]);
		diff_eq_int("SetToneDetect left the second alone (%ld Hz)",
			    (long)ta.tone[1].cfg.freq, 1600L, (long)hz[i]);
		tone_retuned++;
	}
}

/* --------------------------------------------------------------------- */
/* 5.  V32FP_delete, through the allocator's live set.                    */

/*
 * The fourteen things the object releases, in its own order, and a decoy
 * beside each field it reaches so that a neighbouring offset cannot pass.
 */
#define NSLOT		25

struct delplan {
	int	base;		/* 0 obj, 1 hdx, 2 fp                       */
	int	off;
	int	size;
	int	freed;		/* 1 if V32FP_delete should release it      */
	const char *name;
};

static const struct delplan plan[NSLOT] = {
	{ 1, V32_HDX_MTD,	  sizeof(struct fpm_mtd),  1, "hdx mtd" },
	{ 1, V32_HDX_TONE0,	  sizeof(struct fpm_tone), 1, "hdx tone0" },
	{ 1, V32_HDX_TONE1,	  sizeof(struct fpm_tone), 1, "hdx tone1" },
	{ 1, V32_HDX_TONE2,	  sizeof(struct fpm_tone), 1, "hdx tone2" },
	{ 1, V32_HDX_BUF_A4,	  64,			   1, "hdx +0xa4" },
	{ 2, V32FP_BUF_5038,	  64,			   1, "fp +0x5038" },
	{ 2, V32FP_BUF_50CC,	  64,			   1, "fp +0x50cc" },
	{ 2, V32FP_CLEAN_BUF,	  64,			   1, "fp +0x50d0" },
	/* Decoys: one field either side of each pointer the function reads. */
	{ 1, V32_HDX_MTD + 4,	  32,			   0, "hdx +0x3c" },
	{ 1, V32_HDX_TONE0 - 4,	  32,			   0, "hdx +0x28" },
	{ 1, V32_HDX_BUF_A4 + 4,  32,			   0, "hdx +0xa8" },
	{ 1, V32_HDX_BUF_A4 - 4,  32,			   0, "hdx +0xa0" },
	{ 2, V32FP_BUF_5038 + 4,  32,			   0, "fp +0x503c" },
	{ 2, V32FP_BUF_5038 - 4,  32,			   0, "fp +0x5034" },
	{ 2, V32FP_BUF_50CC - 4,  32,			   0, "fp +0x50c8" },
	{ 2, V32FP_CLEAN_BUF + 4, 32,			   0, "fp +0x50d4" },
	/*
	 * Three of `FPM_FSE_free`'s own five, so that freeing the equaliser
	 * from a NEIGHBOURING sub-object's offset leaves them live -- without
	 * these the whole fp block is zero there and both readings free NULL.
	 */
	{ 2, V32FP_FSE + 0x54,	  32,			   1, "fse out_i" },
	{ 2, V32FP_FSE + 0x58,	  32,			   1, "fse out_q" },
	{ 2, V32FP_FSE + 0x60,	  32,			   1, "fse icoeff" },
	{ 2, V32FP_ECC + 0x60,	  32,			   0, "ecc +0x60" },
	{ 2, V32FP_SRE - 4,	  32,			   0, "below the sre" },
	{ 2, V32FP_MRF - 4,	  32,			   0, "below the mrf" },
	{ 2, V32FP_PPS - 4,	  32,			   0, "below the pps" },
	{ 2, V32FP_FSE - 4,	  32,			   0, "below the fse" },
	{ 0, 0x20,		  32,			   0, "obj +0x20" }
};

/*
 * NOT DECOY MATERIAL: an offset INSIDE one of the five sub-objects is the
 * sub-object's own buffer slot and its destructor frees it legitimately, so a
 * decoy there tests `FPM_ECC_free` rather than this function.  The five above
 * sit just OUTSIDE each block, which is the boundary that matters.
 */

/*
 * Build the graph, run one side, and report what survived as a bit pattern
 * plus the allocator's own tally.
 */
struct delresult {
	unsigned long	live;		/* bit per slot                      */
	int		frees;
	int		free_null;
	int		bad_free;
	int		live_count;
	int		obj_live;
	int		hdx_live;
	int		fp_live;
};

static void
run_delete_side(int use_ref, struct delresult *r)
{
	void *slot[NSLOT];
	void *obj, *hdx, *fp;
	struct alloc_log before;
	int i;

	harness_alloc_reset();

	obj = sysdep_malloc(OBJ_SIZE);
	hdx = sysdep_malloc(HDX_SIZE);
	fp = sysdep_malloc(FP_SIZE);
	memset(obj, 0, OBJ_SIZE);
	memset(hdx, 0, HDX_SIZE);
	memset(fp, 0, FP_SIZE);
	put_ptr(obj, V32_OBJ_HDX, hdx);
	put_ptr(obj, V32_OBJ_FP, fp);

	for (i = 0; i < NSLOT; i++) {
		void *base = plan[i].base == 0 ? obj
			   : plan[i].base == 1 ? hdx : fp;

		slot[i] = sysdep_malloc((unsigned)plan[i].size);
		memset(slot[i], 0, (unsigned)plan[i].size);
		put_ptr(base, plan[i].off, slot[i]);
	}

	before = harness_alloc;

	if (use_ref)
		ref_V32FP_delete(obj);
	else
		V32FP_delete(obj);

	r->live = 0;
	for (i = 0; i < NSLOT; i++)
		if (harness_alloc_ordinal(slot[i]) != 0)
			r->live |= 1uL << i;
	r->frees = harness_alloc.frees - before.frees;
	r->free_null = harness_alloc.free_null - before.free_null;
	r->bad_free = harness_alloc.bad_free - before.bad_free;
	r->live_count = harness_alloc.live;
	r->obj_live = harness_alloc_ordinal(obj) != 0;
	r->hdx_live = harness_alloc_ordinal(hdx) != 0;
	r->fp_live = harness_alloc_ordinal(fp) != 0;
}

static void
run_delete(void)
{
	struct delresult ra, rb;
	int i;

	run_delete_side(0, &ra);
	run_delete_side(1, &rb);

	diff_eq_int("V32FP_delete: the surviving set (%ld)", (long)ra.live,
		    (long)rb.live, (long)ra.live);
	diff_eq_int("V32FP_delete: frees (%ld)", (long)ra.frees,
		    (long)rb.frees, (long)ra.frees);
	diff_eq_int("V32FP_delete: sysdep_free(NULL) calls (%ld)",
		    (long)ra.free_null, (long)rb.free_null, (long)ra.free_null);
	diff_eq_int("V32FP_delete: bad frees, ours (%ld)", (long)ra.bad_free,
		    0L, (long)ra.bad_free);
	diff_eq_int("V32FP_delete: bad frees, the blob (%ld)",
		    (long)rb.bad_free, 0L, (long)rb.bad_free);
	diff_eq_int("V32FP_delete: outstanding blocks (%ld)",
		    (long)ra.live_count, (long)rb.live_count,
		    (long)ra.live_count);
	diff_eq_int("V32FP_delete: released the instance (%ld)",
		    (long)ra.obj_live, (long)rb.obj_live, 0L);
	diff_eq_int("V32FP_delete: released the hdx block (%ld)",
		    (long)ra.hdx_live, (long)rb.hdx_live, 0L);
	diff_eq_int("V32FP_delete: released the datapump block (%ld)",
		    (long)ra.fp_live, (long)rb.fp_live, 0L);

	/*
	 * And the plan itself, slot by slot: this is what turns the bit
	 * pattern from "the two agree" into "the two agree AND it is right".
	 */
	for (i = 0; i < NSLOT; i++) {
		char fmt[128];
		int live = (ra.live >> i) & 1u;

		sprintf(fmt, "V32FP_delete: %s %s (slot %%ld)", plan[i].name,
			plan[i].freed ? "was released" : "survived");
		diff_eq_int(fmt, live, plan[i].freed ? 0 : 1, (long)i);
		if (!plan[i].freed && live)
			del_decoys_survived++;
	}
	diff_eq_int("V32FP_delete: our instance is gone (%ld)",
		    (long)ra.obj_live, 0L, (long)ra.obj_live);
	del_runs++;
}

/* --------------------------------------------------------------------- */

int
main(void)
{
	int rc;
	int i;

	diff_begin("v32fpsub");

	run_getdiag();
	run_getdiag_wrapper();
	run_adaptec();
	run_settone();
	run_delete();

	/* ---------------- non-vacuity ---------------- */

	diff_eq_int("FSE_getdiag's diag arm was reached (%ld)", gd_arm[0] > 0,
		    1, gd_arm[0]);
	diff_eq_int("FSE_getdiag's diag2 arm was reached (%ld)", gd_arm[1] > 0,
		    1, gd_arm[1]);
	diff_eq_int("FSE_getdiag's fall-through was reached (%ld)",
		    gd_arm[2] > 0, 1, gd_arm[2]);
	diff_eq_int("FSE_getdiag hit the capacity guard (%ld)",
		    gd_capacity_hit > 0, 1, gd_capacity_hit);
	diff_eq_int("FSE_getdiag was held down to max (%ld)", gd_capped > 0, 1,
		    gd_capped);
	diff_eq_int("FSE_getdiag passed a negative count through (%ld)",
		    gd_negative > 0, 1, gd_negative);
	diff_eq_int("FSE_getdiag copied something (%ld)", gd_copied > 0, 1,
		    gd_copied);

	for (i = 0; i < 4; i++)
		diff_eq_int("SetAdaptEcV32 arm %ld was reached", ec_arm[i] > 0,
			    1, (long)i);
	diff_eq_int("SetAdaptEcV32's fall-through was reached (%ld)",
		    ec_arm[4] > 0, 1, ec_arm[4]);
	diff_eq_int("SetAdaptEcV32 cleared the delay line (%ld)",
		    ec_line_cleared > 0, 1, ec_line_cleared);
	diff_eq_int("SetAdaptEcV32 divided the update gain (%ld)",
		    ec_mu_divided > 0, 1, ec_mu_divided);

	diff_eq_int("SetToneDetect ran (%ld)", tone_retuned > 0, 1,
		    tone_retuned);
	diff_eq_int("V32FP_delete ran (%ld)", del_runs > 0, 1, del_runs);
	diff_eq_int("decoy allocations survived the delete (%ld)",
		    del_decoys_survived > 0, 1, del_decoys_survived);

	rc = diff_end();
	return rc;
}

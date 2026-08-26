/*
 * t_v32demod.c -- differential test of `DemodDataV32`, V.32's receive leaf.
 *
 * The function is a wrapper over five DSP blocks that are each already
 * reconstructed and each already differentially tested, so what can be wrong
 * here is WHICH sub-object, WHICH buffer, WHICH count and WHICH branch -- and
 * every one of those agrees with its wrong reading over most inputs.  The
 * fixture exists to separate them.
 *
 * THE SUB-OBJECTS ARE THE OBJECT'S OWN, built by the blob's `ref_FPM_*_init`
 * from the blob's own `ref_*v32_CFG`, so both sides start from bytes the
 * datapump really produces rather than from something invented here.  The one
 * exception is the equaliser's slicer: `fpm_fse_cfg::decision` is zero in
 * every static instance and is patched by the datapump, so it is a stub in
 * this file -- the same choice `t_fpm_fse_recv.c` makes and for the same
 * reason.  A stub installed identically on both sides is a fixture, not a
 * reconstruction.
 *
 * ---------------------------------------------------------------------------
 * HOW THE BRANCHES ARE REACHED, AND WHY TWO OF THEM ARE NOT FIXTURE-SET
 *
 * `agc.signal` and `sre.active` are written by the callees, not read from the
 * setup, so they cannot simply be assigned.  Two levers make them reachable:
 *
 *   - `agc.signal` is `adjusted > blocks/2` and is written on EVERY call, so
 *     input AMPLITUDE decides it: a block under the squelch leaves it zero.
 *   - `sre.active` is written only where a correlation GROUP completes.  A
 *     call too short to finish one leaves it exactly as the fixture seeded
 *     it, which is what puts the carrier-present arm under control.
 *
 * ---------------------------------------------------------------------------
 * THE TWO ZERO RETURNS THAT LOOK IDENTICAL
 *
 * The low-energy path and the no-carrier path both return 0 and both clear
 * the same status bit, so return value plus status byte cannot tell them
 * apart -- finding F8163's shape exactly.  Two things separate them:
 *
 *   - the low-energy return happens BEFORE `FPM_AGC_agc`, so the AGC, the SRE
 *     and the equaliser have not moved.  The whole `fp` block is compared,
 *     not the return.
 *   - each prints a DIFFERENT string.  The suite runs with the diagnostics
 *     captured at level 3 and compares both transcripts, which is also what
 *     makes the two gates -- `> 1` for one and `> 2` for the other --
 *     separable at all.
 *
 * ---------------------------------------------------------------------------
 * WHY THE `fp` COMPARISON IS MASKED
 *
 * The five `FPM_*_init` calls allocate eight-plus buffers each, so the two
 * fixtures hold different addresses in their pointer fields and always will.
 * The mask is BUILT rather than declared: every byte that differs between the
 * two blocks immediately after construction is skipped, and the count of
 * masked bytes is asserted to be small and non-zero, so a mask that swallowed
 * the block would fail rather than pass silently.  What the pointers POINT AT
 * is compared separately and unmasked.
 */

#include <string.h>

#include "harness.h"
#include "dsplib/v32data.h"
#include "dsplib/v32demod.h"
#include "dsplib/v32hdx.h"
#include "dsplib/debug.h"
#include "dsplib/fpm.h"
#include "dsplib/fpm_agc.h"
#include "dsplib/fpm_ecc.h"
#include "dsplib/fpm_fse.h"
#include "dsplib/fpm_mrf.h"
#include "dsplib/fpm_sre.h"

extern unsigned short ref_DemodDataV32(void *modem, short *in,
				       unsigned short *out,
				       unsigned short count);

extern void ref_FPM_MRF_init(struct fpm_mrf *s, const struct fpm_mrf_cfg *c,
			     int fresh);
extern void ref_FPM_ECC_init(struct fpm_ecc *s, const struct fpm_ecc_cfg *c,
			     int fresh);
extern void ref_FPM_SRE_init(struct fpm_sre *s, const struct fpm_sre_cfg *c,
			     int fresh);
extern void ref_FPM_AGC_init(struct fpm_agc *s, const struct fpm_agc_cfg *c,
			     int reset);
extern void ref_FPM_FSE_init(struct fpm_fse *s, const struct fpm_fse_cfg *c,
			     int fresh);

extern const struct fpm_mrf_cfg ref_MRFv32_CFG;
extern const struct fpm_ecc_cfg ref_ECCv32_CFG;
extern const struct fpm_sre_cfg ref_SREv32_CFG;
extern struct fpm_agc_cfg ref_AGCv32_CFG;
extern struct fpm_fse_cfg ref_FSEv32_CFG;

extern unsigned int ref_dsplibs_debug_level;

/* --------------------------------------------------------------------- */

#define OBJ_SIZE	0x80
#define HDX_SIZE	0x100
#define FP_SIZE		0x50e0		/* past V32FP_CLEANLEN + 2       */

/* V32FP_recreate gives each buffer 0x154 bytes; the slack is a canary. */
#define BUFN		170
#define BUFSLACK	64
#define ION		1024

#define ECC_DELAY	40		/* fpm_ecc's two INPUT fields     */
#define CANARY_RX	((short)0x3c3c)
#define CANARY_CL	((short)0x6d6d)

struct fp_image { unsigned char b[FP_SIZE]; };
struct buf_image { short s[BUFN + BUFSLACK]; };
struct io_image { short s[ION]; };
struct out_image { unsigned short u[ION]; };

struct fix {
	double		align;
	unsigned char	obj[OBJ_SIZE];
	unsigned char	hdx[HDX_SIZE];
	unsigned char	fp[FP_SIZE];
	short		rxbuf[BUFN + BUFSLACK];
	short		clean[BUFN + BUFSLACK];
	short		io[ION];
	unsigned short	out[ION];
};

static struct fix fa, fb;
static unsigned char fpmask[FP_SIZE];
static long fpmask_bytes;

/* --------------------------------------------------------------------- */

struct trial {
	const char	*what;
	unsigned	seed;
	short		mode;		/* hdx + 0x76                    */
	short		rms_min;	/* obj + 0x28                    */
	unsigned char	status;		/* obj + 0x31 on entry           */
	int		adapt_near;
	int		adapt_far;
	int		sre_active;	/* pre-seeded; see the header    */
	short		sre_mode;	/* pre-seeded                    */
	int		en_sre;
	int		en_pll;
	int		en_tilt;
	int		en_lms;
	int		agc_f18;
	int		amp;		/* input amplitude               */
	unsigned short	count;
};

/*
 * `diff_begin` ZEROES the failure count, so a section whose `diff_end` is
 * discarded reports FAIL and exits 0.  Every section accumulates here.
 */
static int rc_total;

/* Non-vacuity counters, every one asserted non-zero in main(). */
static long hit_n_zero, hit_n_pos;
static long hit_ec_gate;
static long hit_low_energy, hit_no_carrier;
static long hit_mode6_rms_pass, hit_mode_other;
static long hit_sre_mode0, hit_sre_mode_nz;
static long hit_silence_set, hit_silence_clear;
static long hit_carrier_set, hit_carrier_clear;
static long hit_fse_output;
static long hit_status_other_bits;
static long hit_masked_enable, hit_unmasked_enable;
static long hit_transcript_lines;
static long hit_floor_exact;
static long hit_tilt_lms_distinct;
static long hit_carrier_cleared_from_set;

/* --------------------------------------------------------------------- */

static void
put_ptr(unsigned char *p, int off, void *v)
{
	*(void **)(void *)(p + off) = v;
}

static void
put_short(unsigned char *p, int off, short v)
{
	*(short *)(void *)(p + off) = v;
}

static void
put_int(unsigned char *p, int off, int v)
{
	*(int *)(void *)(p + off) = v;
}

static int
get_int(const unsigned char *p, int off)
{
	return *(const int *)(const void *)(p + off);
}

static short
get_short(const unsigned char *p, int off)
{
	return *(const short *)(const void *)(p + off);
}

static struct fpm_agc *
agc_of(struct fix *f)
{
	return (struct fpm_agc *)(void *)(f->fp + V32FP_AGC);
}

static struct fpm_ecc *
ecc_of(struct fix *f)
{
	return (struct fpm_ecc *)(void *)(f->fp + V32FP_ECC);
}

static struct fpm_fse *
fse_of(struct fix *f)
{
	return (struct fpm_fse *)(void *)(f->fp + V32FP_FSE);
}

static struct fpm_sre *
sre_of(struct fix *f)
{
	return (struct fpm_sre *)(void *)(f->fp + V32FP_SRE);
}

/* --------------------------------------------------------------------- */

/*
 * The slicer.  Stateless and deliberately NOT a real V.32 decision: it must
 * be identical on both sides and must not agree with itself by accident, so
 * it reports a magnitude derived from the measured one and rotates the angle
 * by a fixed amount.  `t_fpm_fse_recv.c` makes the same choice.
 */
static unsigned short
stub_slicer(struct fpm_fse *state, short *angle, short *mag)
{
	short a = *angle;

	(void)state;
	*angle = (short)(a - 0x0140);
	*mag = (short)(0x2000 + (a >> 4));
	return (unsigned short)((unsigned short)a & 0x0f);
}

/* --------------------------------------------------------------------- */

static unsigned
rng(unsigned *s)
{
	*s ^= *s << 13;
	*s ^= *s >> 17;
	*s ^= *s << 5;
	return *s;
}

static void
build(struct fix *f, const struct trial *t)
{
	struct fpm_fse_cfg fsecfg;
	struct fpm_ecc *ecc;
	struct fpm_sre *sre;
	unsigned s = t->seed ? t->seed : 1u;
	int i;

	memset(f, 0, sizeof(*f));

	/*
	 * A tone at 1800 Hz -- V.32's carrier -- plus a little noise, so the
	 * echo canceller, the AGC and the timing recovery all see something
	 * with structure rather than a flat line.  Amplitude is the lever the
	 * header describes.
	 */
	for (i = 0; i < ION; i++) {
		int v = (i % 40) < 20 ? t->amp : -t->amp;

		v += (int)(rng(&s) % 129u) - 64;
		if (v > 32767)
			v = 32767;
		if (v < -32768)
			v = -32768;
		f->io[i] = (short)v;
	}

	/*
	 * TWO DIFFERENT canaries.  With one, a copy that runs a single element
	 * past `n` lands the same value the slot already held and no
	 * comparison fires -- measured, not supposed: that mutation survived
	 * until these were split.
	 */
	for (i = 0; i < BUFN + BUFSLACK; i++) {
		f->rxbuf[i] = CANARY_RX;
		f->clean[i] = CANARY_CL;
	}
	for (i = 0; i < ION; i++)
		f->out[i] = 0x9e9e;

	put_ptr(f->obj, V32_OBJ_HDX, f->hdx);
	put_ptr(f->obj, V32_OBJ_FP, f->fp);
	put_short(f->obj, V32_OBJ_RMS_MIN, t->rms_min);
	f->obj[V32_OBJ_STATUS] = t->status;

	put_short(f->hdx, V32HDX_MODE, t->mode);

	/* The two ECC delays are INPUTS init reads; see fpm_ecc.h. */
	ecc = ecc_of(f);
	ecc->near_delay = ECC_DELAY;
	ecc->far_delay = ECC_DELAY;

	ref_FPM_MRF_init((struct fpm_mrf *)(void *)(f->fp + V32FP_MRF),
			 &ref_MRFv32_CFG, 1);
	ref_FPM_ECC_init(ecc, &ref_ECCv32_CFG, 1);
	ref_FPM_SRE_init(sre_of(f), &ref_SREv32_CFG, 1);
	ref_FPM_AGC_init(agc_of(f), &ref_AGCv32_CFG, 1);

	fsecfg = ref_FSEv32_CFG;
	fsecfg.decision = stub_slicer;
	ref_FPM_FSE_init(fse_of(f), &fsecfg, 1);

	ecc->adapt_near = t->adapt_near;
	ecc->adapt_far = t->adapt_far;

	sre = sre_of(f);
	sre->active = t->sre_active;
	sre->mode = t->sre_mode;

	agc_of(f)->f18 = t->agc_f18;

	put_int(f->fp, V32FP_SRE_ADAPT_EN, t->en_sre);
	put_int(f->fp, V32FP_FSE_PLL_EN, t->en_pll);
	put_int(f->fp, V32FP_FSE_TILT_EN, t->en_tilt);
	put_int(f->fp, V32FP_FSE_LMS_EN, t->en_lms);

	put_ptr(f->fp, V32FP_RXBUF, f->rxbuf);
	put_ptr(f->fp, V32FP_CLEAN, f->clean);
	put_short(f->fp, V32FP_RXLEN, (short)0x5a5a);
	put_short(f->fp, V32FP_CLEANLEN, (short)0x5a5a);
}

/* --------------------------------------------------------------------- */

/*
 * The first byte at which the two blocks differ, masked bytes skipped, or -1.
 * One check per object rather than one per byte, and it names the offset --
 * the argument `diff_eq_obj` is built on, applied to a region that needs a
 * skip list.
 */
static int
first_diff(const unsigned char *a, const unsigned char *b, int n,
	   const unsigned char *mask)
{
	int i;

	for (i = 0; i < n; i++) {
		if (mask != 0 && mask[i])
			continue;
		if (a[i] != b[i])
			return i;
	}
	return -1;
}

static void
build_mask(void)
{
	int i;

	fpmask_bytes = 0;
	for (i = 0; i < FP_SIZE; i++) {
		fpmask[i] = (unsigned char)(fa.fp[i] != fb.fp[i]);
		if (fpmask[i])
			fpmask_bytes++;
	}
}

/*
 * The eight-plus heap regions the five sub-objects own.  Lengths come from
 * the (identical, unmasked) size fields beside each pointer, so a wrong
 * sub-object base would produce a wrong length here too and the comparison
 * would still fire.
 */
static void
compare_heap(long trial)
{
	struct fpm_ecc *ea = ecc_of(&fa), *eb = ecc_of(&fb);
	struct fpm_sre *sa = sre_of(&fa), *sb = sre_of(&fb);
	struct fpm_fse *xa = fse_of(&fa), *xb = fse_of(&fb);
	int coefn = 2 * (ea->cfg.near_taps + ea->cfg.far_taps);
	int j;

	diff_eq_int("trial %ld: ecc line", first_diff((unsigned char *)ea->line,
		    (unsigned char *)eb->line, 2 * ea->line_len, 0), -1,
		    trial);
	diff_eq_int("trial %ld: ecc near_i",
		    first_diff((unsigned char *)ea->near_i,
			       (unsigned char *)eb->near_i, 2 * ea->near_len,
			       0), -1, trial);
	diff_eq_int("trial %ld: ecc near_q",
		    first_diff((unsigned char *)ea->near_q,
			       (unsigned char *)eb->near_q, 2 * ea->near_len,
			       0), -1, trial);
	diff_eq_int("trial %ld: ecc far_i",
		    first_diff((unsigned char *)ea->far_i,
			       (unsigned char *)eb->far_i, 2 * ea->far_len, 0),
		    -1, trial);
	diff_eq_int("trial %ld: ecc far_q",
		    first_diff((unsigned char *)ea->far_q,
			       (unsigned char *)eb->far_q, 2 * ea->far_len, 0),
		    -1, trial);
	for (j = 0; j < 3; j++)
		diff_eq_int("trial %ld: ecc coefficient bank",
			    first_diff((unsigned char *)ea->coef[j],
				       (unsigned char *)eb->coef[j],
				       2 * coefn, 0), -1, trial);

	diff_eq_int("trial %ld: sre coefficients",
		    first_diff((unsigned char *)sa->coeff,
			       (unsigned char *)sb->coeff, 2 * sa->cfg.coeffs,
			       0), -1, trial);
	diff_eq_int("trial %ld: sre history",
		    first_diff((unsigned char *)sa->hist,
			       (unsigned char *)sb->hist, 2 * sa->taps, 0),
		    -1, trial);
	diff_eq_int("trial %ld: sre rms buffer",
		    first_diff((unsigned char *)sa->rms_buf,
			       (unsigned char *)sb->rms_buf,
			       2 * sa->cfg.rms_len, 0), -1, trial);

	diff_eq_int("trial %ld: fse I coefficients",
		    first_diff((unsigned char *)xa->icoeff,
			       (unsigned char *)xb->icoeff, 2 * xa->cfg.taps,
			       0), -1, trial);
	diff_eq_int("trial %ld: fse Q coefficients",
		    first_diff((unsigned char *)xa->qcoeff,
			       (unsigned char *)xb->qcoeff, 2 * xa->cfg.taps,
			       0), -1, trial);
	diff_eq_int("trial %ld: fse history",
		    first_diff((unsigned char *)xa->hist,
			       (unsigned char *)xb->hist, 2 * xa->cfg.taps, 0),
		    -1, trial);
}

/* --------------------------------------------------------------------- */

static void
tally(const struct trial *t, unsigned short ret)
{
	const char *tx = dsplib_debug_capture_text(1);
	unsigned char st = fb.obj[V32_OBJ_STATUS];
	int low, nocar;

	if (tx == 0)
		tx = "";
	low = strstr(tx, "low sig energy") != 0;
	nocar = strstr(tx, "sre no carrier") != 0;

	if (tx[0] != '\0')
		hit_transcript_lines++;
	if (get_short(fb.fp, V32FP_RXLEN) == 0)
		hit_n_zero++;
	else
		hit_n_pos++;
	if ((t->adapt_near != 0 || t->adapt_far != 0) && ret == 0)
		hit_ec_gate++;
	if (low)
		hit_low_energy++;
	if (nocar)
		hit_no_carrier++;
	if (t->mode == V32_MODE_6 && !low && t->adapt_near == 0
	    && t->adapt_far == 0)
		hit_mode6_rms_pass++;
	if (t->mode != V32_MODE_6)
		hit_mode_other++;
	if ((st & V32_STATUS_SILENCE) != 0)
		hit_silence_set++;
	else
		hit_silence_clear++;
	if ((st & V32_STATUS_CARRIER) != 0)
		hit_carrier_set++;
	else
		hit_carrier_clear++;
	if (ret > 0)
		hit_fse_output++;
	/*
	 * The two arms that no earlier fixture reached: the mode-6 else arm
	 * with the two equaliser enables landing on DIFFERENT values (so
	 * transposing them shows), and the no-carrier arm entered with the
	 * carrier bit already SET (so the clear is not a no-op).
	 */
	if ((st & V32_STATUS_CARRIER) != 0 && t->mode == V32_MODE_6
	    && fse_of(&fb)->tilt_on != fse_of(&fb)->lms_on)
		hit_tilt_lms_distinct++;
	if (nocar && (t->status & V32_STATUS_CARRIER) != 0)
		hit_carrier_cleared_from_set++;
	if ((st & (unsigned char)~(V32_STATUS_SILENCE | V32_STATUS_CARRIER))
	    != 0)
		hit_status_other_bits++;

	/*
	 * Did the run reach the arm where an enable is masked by `agc.f18`,
	 * and separately the arm where `lms_on` is NOT?  Both are read off
	 * the equaliser afterwards, which is where the branch writes.
	 */
	if ((st & V32_STATUS_CARRIER) != 0) {
		if (t->mode == V32_MODE_6) {
			if (t->sre_mode == 0)
				hit_sre_mode0++;
			else
				hit_sre_mode_nz++;
			if (t->agc_f18 == 0 && t->en_tilt != 0)
				hit_masked_enable++;
		} else if (t->agc_f18 == 0 && t->en_lms != 0
			   && fse_of(&fb)->lms_on != 0) {
			hit_unmasked_enable++;
		}
	}
}

/* --------------------------------------------------------------------- */

static void
run_at(const struct trial *t, long trial, unsigned level)
{
	unsigned short ra, rb;
	int i;

	build(&fa, t);
	build(&fb, t);
	build_mask();

	dsplib_debug_capture_reset();
	dsplib_debug_capture_on = 1;
	dsplibs_debug_level = level;
	ref_dsplibs_debug_level = level;

	ra = DemodDataV32(fa.obj, fa.io, fa.out, t->count);
	rb = ref_DemodDataV32(fb.obj, fb.io, fb.out, t->count);

	dsplib_debug_capture_on = 0;
	dsplibs_debug_level = 0;
	ref_dsplibs_debug_level = 0;

	diff_begin(t->what);

	diff_eq_int("return (%ld)", ra, rb, trial);
	diff_eq_int("status byte (%ld)", fa.obj[V32_OBJ_STATUS],
		    fb.obj[V32_OBJ_STATUS], trial);
	diff_eq_int("V32FP_RXLEN (%ld)", get_short(fa.fp, V32FP_RXLEN),
		    get_short(fb.fp, V32FP_RXLEN), trial);
	diff_eq_int("V32FP_CLEANLEN (%ld)", get_short(fa.fp, V32FP_CLEANLEN),
		    get_short(fb.fp, V32FP_CLEANLEN), trial);

	/* The fields the branches write, named rather than left to a sweep. */
	diff_eq_int("sre.adapt (%ld)", sre_of(&fa)->adapt,
		    sre_of(&fb)->adapt, trial);
	diff_eq_int("sre.active (%ld)", sre_of(&fa)->active,
		    sre_of(&fb)->active, trial);
	diff_eq_int("fse.pll_on (%ld)", fse_of(&fa)->pll_on,
		    fse_of(&fb)->pll_on, trial);
	diff_eq_int("fse.tilt_on (%ld)", fse_of(&fa)->tilt_on,
		    fse_of(&fb)->tilt_on, trial);
	diff_eq_int("fse.lms_on (%ld)", fse_of(&fa)->lms_on,
		    fse_of(&fb)->lms_on, trial);
	diff_eq_int("agc.signal (%ld)", agc_of(&fa)->signal,
		    agc_of(&fb)->signal, trial);
	diff_eq_int("the enables were left alone (%ld)",
		    get_int(fa.fp, V32FP_FSE_LMS_EN),
		    get_int(fb.fp, V32FP_FSE_LMS_EN), trial);

	diff_eq_obj("the working buffer", struct buf_image, fa.rxbuf, fb.rxbuf,
		    trial);
	diff_eq_obj("the cleaned copy", struct buf_image, fa.clean, fb.clean,
		    trial);
	diff_eq_obj("the in/out sample buffer", struct io_image, fa.io, fb.io,
		    trial);
	diff_eq_obj("the decision buffer", struct out_image, fa.out, fb.out,
		    trial);

	diff_eq_int("trial %ld: first differing fp byte",
		    first_diff(fa.fp, fb.fp, FP_SIZE, fpmask), -1, trial);
	compare_heap(trial);

	/* The mask must be a handful of pointers, not the block. */
	diff_eq_int("masked bytes are few (%ld)",
		    fpmask_bytes > 0 && fpmask_bytes < 256, 1, fpmask_bytes);

	/* Neither buffer may be written past the 0x154 bytes it was given. */
	for (i = BUFN; i < BUFN + BUFSLACK; i++) {
		diff_eq_int("rxbuf canary %ld", fb.rxbuf[i], CANARY_RX, i);
		diff_eq_int("clean canary %ld", fb.clean[i], CANARY_CL, i);
	}

	/* Tier 4: the two sides must say the same things. */
	{
		const char *ta = dsplib_debug_capture_text(0);
		const char *tb = dsplib_debug_capture_text(1);

		if (ta == 0)
			ta = "";
		if (tb == 0)
			tb = "";
		diff_eq_int("transcript (%ld)", strcmp(ta, tb) == 0, 1, trial);
	}

	if (level == 3)
		tally(t, rb);

	rc_total |= diff_end();
}

/*
 * Every trial at level 1, 2 and 3.  `debug.h`: "a site at the wrong threshold
 * produces a byte-identical transcript at level 2, and one macro for every
 * gate quietly flattens the distinction".  This function's two gates are `> 1`
 * and `> 2`, so level 2 is the one that separates them and a sweep is the only
 * thing that visits it.
 */
static void
run_one(const struct trial *t, long trial)
{
	unsigned level;

	for (level = 1; level <= 3; level++)
		run_at(t, trial * 4 + (long)level, level);
}

/* --------------------------------------------------------------------- */

#define LOUD	7000
#define QUIET	20

static const struct trial trials[] = {
/*   what                                       seed  mode rms  st   an af act sm  sre pll tlt lms  f18   amp  cnt */
{ "quiet line, no carrier seeded",        0x1001u,  0,    0, 0x00, 0, 0, 0,  0,  1,  1,  1,  1,   1, QUIET, 120 },
{ "loud line, carrier seeded",            0x1002u,  0,    0, 0x00, 0, 0, 1,  0,  1,  1,  1,  1,   1, LOUD,  120 },
{ "loud line, carrier, other status bits", 0x1003u, 0,    0, 0x8b, 0, 0, 1,  1,  1,  1,  1,  1,   1, LOUD,  120 },
{ "mode 6, floor at zero, sre mode 0",    0x1004u,  6,    0, 0x00, 0, 0, 1,  0,  1,  1,  1,  1,   1, LOUD,  120 },
{ "mode 6, floor at zero, sre mode 1",    0x1005u,  6,    0, 0x00, 0, 0, 1,  1,  1,  1,  1,  1,   1, LOUD,  120 },
{ "mode 6, floor at zero, sre mode 2",    0x1006u,  6,    0, 0x00, 0, 0, 1,  2,  1,  1,  1,  1,   1, LOUD,  120 },
{ "mode 6, floor above the block",        0x1007u,  6, 32767, 0x7f, 0, 0, 1, 1,  1,  1,  1,  1,   1, LOUD,  120 },
{ "mode 6, floor above a quiet block",    0x1008u,  6,  1000, 0x20, 0, 0, 0, 0,  1,  1,  1,  1,   1, QUIET, 120 },
{ "the near canceller is still adapting", 0x1009u,  0,    0, 0xff, 1, 0, 1,  1,  1,  1,  1,  1,   1, LOUD,  120 },
{ "the far canceller is still adapting",  0x100au,  6,    0, 0x00, 0, 1, 1,  1,  1,  1,  1,  1,   1, LOUD,  120 },
{ "both cancellers adapting",             0x100bu,  0,    0, 0x60, 1, 1, 1,  0,  1,  1,  1,  1,   1, LOUD,  120 },
{ "a count too short to make a sample",   0x100cu,  0,    0, 0x00, 0, 0, 1,  0,  1,  1,  1,  1,   1, LOUD,    0 },
{ "a count of one",                       0x100du,  0,    0, 0x00, 0, 0, 1,  0,  1,  1,  1,  1,   1, LOUD,    1 },
{ "f18 zero masks three enables",         0x100eu,  6,    0, 0x00, 0, 0, 1,  1,  1,  1,  1,  1,   0, LOUD,  120 },
{ "f18 zero, mode not 6: lms is unmasked", 0x100fu, 0,    0, 0x00, 0, 0, 1,  1,  1,  1,  1,  1,   0, LOUD,  120 },
{ "f18 all ones",                         0x1010u,  6,    0, 0x00, 0, 0, 1,  2, -1, -1, -1, -1,  -1, LOUD,  120 },
{ "distinct enables, mode 6",             0x1011u,  6,    0, 0x00, 0, 0, 1,  2, 0x11, 0x22, 0x44, 0x88, 0xff, LOUD,  120 },
{ "distinct enables, mode not 6",         0x1012u,  1,    0, 0x00, 0, 0, 1,  2, 0x11, 0x22, 0x44, 0x88, 0xff, LOUD,  120 },
/* Short enough that no SRE group completes, so `active` survives as seeded. */
{ "distinct enables, mode 6, short block", 0x101du, 6,    0, 0x00, 0, 0, 1,  2, 0x11, 0x22, 0x44, 0x88, 0xff, LOUD,   12 },
{ "distinct enables, mode 6, timing mode 1", 0x101eu, 6,  0, 0x00, 0, 0, 1, 1, 0x11, 0x22, 0x44, 0x88, 0xff, LOUD,   12 },
{ "distinct enables, not 6, short block", 0x101fu,  0,    0, 0x00, 0, 0, 1,  2, 0x11, 0x22, 0x44, 0x88, 0xff, LOUD,   12 },
/*
 * `en_sre` 0 makes `sre.adapt` 0, and `FPM_SRE_recover`'s phase update returns
 * on that BEFORE its settling branch forces `sre.mode` to 0 -- which is why
 * every earlier mode-6 trial came out of the `mode == 0` arm with the two
 * equaliser enables equal, and why transposing them was invisible.
 */
{ "mode 6, timing frozen, timing mode 2", 0x1022u,  6,    0, 0x00, 0, 0, 1,  2,  0, 0x22, 0x44, 0x88, 0xff, LOUD,  120 },
{ "mode 6, timing frozen, timing mode 1", 0x1023u,  6,    0, 0x00, 0, 0, 1,  1,  0, 0x22, 0x44, 0x88, 0xff, LOUD,  120 },
{ "mode 6, timing frozen, short block",   0x1024u,  6,    0, 0x0f, 0, 0, 1,  2,  0, 0x22, 0x44, 0x88, 0xff, LOUD,   12 },
/*
 * A single input sample makes the resampler produce nothing, so the timing
 * recovery is handed a zero count and leaves `active` exactly as seeded --
 * the only shape that reaches the no-carrier arm with the bit already set.
 */
{ "no carrier from one sample, bit set",  0x1025u,  0,    0, 0xff, 0, 0, 0,  0, 0x11, 0x22, 0x44, 0x88, 0xff, LOUD,    1 },
{ "no carrier from one sample, bit only", 0x1026u,  2,    0, 0x20, 0, 0, 0,  1, 0x11, 0x22, 0x44, 0x88, 0xff, QUIET,   1 },
/* The carrier bit is ALREADY SET on entry and the arm must clear it. */
{ "no carrier, carrier bit set on entry", 0x1020u,  0,    0, 0xff, 0, 0, 0,  0, 0x11, 0x22, 0x44, 0x88, 0xff, QUIET, 120 },
{ "no carrier, only the carrier bit set", 0x1021u,  1,    0, 0x20, 0, 0, 0,  1, 0x11, 0x22, 0x44, 0x88, 0xff, QUIET, 120 },
{ "every enable zero",                    0x1013u,  6,    0, 0x00, 0, 0, 1,  1,  0,  0,  0,  0,   1, LOUD,  120 },
{ "mode 5, one below the tested value",   0x1014u,  5,    0, 0x00, 0, 0, 1,  1,  1,  1,  1,  1,   1, LOUD,  120 },
{ "mode 7, one above it",                 0x1015u,  7,    0, 0x00, 0, 0, 1,  1,  1,  1,  1,  1,   1, LOUD,  120 },
{ "a negative mode",                      0x1016u, -1,    0, 0x00, 0, 0, 1,  1,  1,  1,  1,  1,   1, LOUD,  120 },
{ "a negative floor",                     0x1017u,  6,   -1, 0x00, 0, 0, 1,  1,  1,  1,  1,  1,   1, QUIET, 120 },
{ "a short block, loud",                  0x1018u,  6,    0, 0x00, 0, 0, 1,  1,  1,  1,  1,  1,   1, LOUD,   12 },
{ "a short block, quiet",                 0x1019u,  0,    0, 0x00, 0, 0, 1,  0,  1,  1,  1,  1,   1, QUIET,  12 },
{ "a full-length block",                  0x101au,  0,    0, 0x00, 0, 0, 1,  0,  1,  1,  1,  1,   1, LOUD,  150 },
{ "mid amplitude, mode 6",                0x101bu,  6,  4000, 0x00, 0, 0, 1, 1,  1,  1,  1,  1,   1,  1200, 120 },
{ "mid amplitude, mode not 6",            0x101cu,  0,  4000, 0x00, 0, 0, 1, 1,  1,  1,  1,  1,   1,  1200, 120 }
};

/* --------------------------------------------------------------------- */

/*
 * The floor comparison is `>`, and `>=` differs on exactly one value of
 * `rms_min`: the block's own RMS.  That value is not known in advance, so it
 * is MEASURED first -- a floor above everything forces the low-energy return
 * BEFORE `FPM_AGC_agc`, which leaves the working buffer holding the cancelled
 * block untouched -- and then the same trial is re-run at r-1, r and r+1.
 * Without the exact value neither comparison can ever be separated.
 */
static void
run_floor_boundary(long base)
{
	struct trial t = trials[3];		/* mode 6, loud, count 120   */
	short r;

	t.seed = 0x2001u;
	t.rms_min = 32767;

	dsplib_debug_capture_on = 0;
	dsplibs_debug_level = 0;
	ref_dsplibs_debug_level = 0;

	build(&fb, &t);
	(void)ref_DemodDataV32(fb.obj, fb.io, fb.out, t.count);
	r = FPM_rms(fb.rxbuf, (unsigned short)get_short(fb.fp, V32FP_RXLEN));

	t.rms_min = (short)(r - 1);
	run_one(&t, base);
	t.rms_min = r;
	run_one(&t, base + 1);
	hit_floor_exact++;
	t.rms_min = (short)(r + 1);
	run_one(&t, base + 2);
}

int
main(void)
{
	int rc = 0;
	long i;

	for (i = 0; i < (long)(sizeof(trials) / sizeof(trials[0])); i++)
		run_one(&trials[i], i);
	run_floor_boundary(1000);

	diff_begin("v32demod separating trials");
	diff_eq_int("the resampler returned zero (%ld)", hit_n_zero > 0, 1,
		    hit_n_zero);
	diff_eq_int("the resampler returned samples (%ld)", hit_n_pos > 0, 1,
		    hit_n_pos);
	diff_eq_int("the canceller gate declined a block (%ld)",
		    hit_ec_gate > 0, 1, hit_ec_gate);
	diff_eq_int("the low-energy arm was taken (%ld)", hit_low_energy > 0,
		    1, hit_low_energy);
	diff_eq_int("the no-carrier arm was taken (%ld)", hit_no_carrier > 0,
		    1, hit_no_carrier);
	diff_eq_int("mode 6 passed the floor (%ld)", hit_mode6_rms_pass > 0, 1,
		    hit_mode6_rms_pass);
	diff_eq_int("a mode other than 6 was driven (%ld)", hit_mode_other > 0,
		    1, hit_mode_other);
	diff_eq_int("the sre mode 0 arm was taken (%ld)", hit_sre_mode0 > 0, 1,
		    hit_sre_mode0);
	diff_eq_int("the sre mode non-zero arm was taken (%ld)",
		    hit_sre_mode_nz > 0, 1, hit_sre_mode_nz);
	diff_eq_int("the silence bit was set (%ld)", hit_silence_set > 0, 1,
		    hit_silence_set);
	diff_eq_int("the silence bit was cleared (%ld)", hit_silence_clear > 0,
		    1, hit_silence_clear);
	diff_eq_int("the carrier bit was set (%ld)", hit_carrier_set > 0, 1,
		    hit_carrier_set);
	diff_eq_int("the carrier bit was cleared (%ld)", hit_carrier_clear > 0,
		    1, hit_carrier_clear);
	diff_eq_int("the equaliser produced symbols (%ld)", hit_fse_output > 0,
		    1, hit_fse_output);
	diff_eq_int("status bits outside the two survived (%ld)",
		    hit_status_other_bits > 0, 1, hit_status_other_bits);
	diff_eq_int("an enable was masked to zero by f18 (%ld)",
		    hit_masked_enable > 0, 1, hit_masked_enable);
	diff_eq_int("lms_on survived f18 == 0 (%ld)", hit_unmasked_enable > 0,
		    1, hit_unmasked_enable);
	diff_eq_int("the diagnostics printed something (%ld)",
		    hit_transcript_lines > 0, 1, hit_transcript_lines);
	diff_eq_int("the floor was driven at exactly the block's RMS (%ld)",
		    hit_floor_exact > 0, 1, hit_floor_exact);
	diff_eq_int("mode 6 left the two equaliser enables unequal (%ld)",
		    hit_tilt_lms_distinct > 0, 1, hit_tilt_lms_distinct);
	diff_eq_int("the no-carrier arm cleared a bit that was set (%ld)",
		    hit_carrier_cleared_from_set > 0, 1,
		    hit_carrier_cleared_from_set);
	rc |= diff_end();

	return rc | rc_total;
}

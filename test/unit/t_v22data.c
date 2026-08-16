/*
 * t_v22data.c -- differential test of V.22's data-path leaves.
 *
 * These four are wrappers: they pick a sub-object out of an unmodelled
 * datapump instance and call the module that owns it.  A wrapper has almost
 * no arithmetic of its own, so "it agrees with the blob over a thousand
 * random inputs" is nearly worthless -- the thousand inputs are being carried
 * by the callee, which has its own test.  What can actually be wrong is WHICH
 * sub-object, and a wrong offset agrees with the right one over every input
 * unless the fixture makes the two objects behave differently.
 *
 * So every check here is built around a NAMED WRONG READING, and the count of
 * trials that SEPARATE it from ours is asserted non-zero at the end.  Finding
 * 3052 is what that is for.  The wrong readings, in order:
 *
 *   ScrambleDataV22 / DescrambleDataV22
 *       - the wrong FP offset (0x30 against 0x1cc)
 *       - scrambler and descrambler swapped
 *     Both are evaluated for real, by driving the OTHER object (or the other
 *     entry point) with `ref_` on a copy and comparing.  The two `fpm_sdm`
 *     objects therefore carry DIFFERENT configurations and DIFFERENT shift
 *     registers, without which neither reading could ever be told apart.
 *
 *   ModDataV22
 *       - the ring not shared between the encoder and the filter.  Evaluated
 *         for real: encode into one ring, filter from another.
 *       - the return sign-extended rather than zero-extended.  Counted as the
 *         trials whose sample count has bit 15 set, which needs more than
 *         2,458 symbols in one call and so has a trial of its own.
 *       - the two FP offsets swapped (0x48 against 0x78).  NOT evaluated, and
 *         deliberately: `V22_PPS_filter` would read `coeff_i` out of
 *         `fpm_smc.cfg.direct`, which is zero for V.22, and dereference null.
 *         That reading FAULTS rather than diverging, so it is separated by
 *         construction and there is nothing to count.  What is covered
 *         instead is the near miss -- the whole FP block is filled with a
 *         pseudorandom pattern before the sub-objects are laid into it, so an
 *         offset wrong by a few bytes reads garbage and diverges, and both
 *         sub-object regions are asserted to have been written.
 *
 *   Detect_v22
 *       - the two counters paired with the wrong detectors.  THE RETURN
 *         CANNOT SEE THIS: it is `(a > 2) | (b > 2)`, symmetric in the two.
 *         The asymmetry is that the buffer clear is gated on the FIRST
 *         detector's counter and the diagnostic on the SECOND, so the test
 *         compares the whole 160-sample buffer and the debug transcript.
 *       - the clear gated on the verdict alone rather than on the counter,
 *         which differs only on the first quiet sub-block after a noisy one.
 *       - `>= 2` in place of `> 2`.
 *     All three are evaluated for real by local drivers built out of the
 *     `ref_` callees, which is safe because the fixture layout is identical.
 *
 * The instance is not modelled (see include/dsplib/v22data.h), so the
 * fixtures are byte buffers with the known offsets poked directly, exactly as
 * `t_v22prc.c` does.
 */

#include <math.h>
#include <string.h>

#include "harness.h"
#include "dsplib/v22data.h"
#include "dsplib/debug.h"
#include "dsplib/fpm_agc.h"
#include "dsplib/fpm_mtd.h"
#include "dsplib/fpm_sdm.h"
#include "dsplib/fpm_smc.h"
#include "dsplib/v22_pps.h"

extern void ref_ScrambleDataV22(void *modem, unsigned short *data,
				unsigned short count);
extern void ref_DescrambleDataV22(void *modem, unsigned short *data,
				  unsigned short count);
extern unsigned short ref_ModDataV22(void *modem, const unsigned short *data,
				     short *out, unsigned short count);
extern int ref_Detect_v22(void *modem, short *data);

extern void ref_FPM_SDM_scrambler(struct fpm_sdm *sdm, unsigned short *data,
				  unsigned short count);
extern void ref_FPM_SDM_descrambler(struct fpm_sdm *sdm, unsigned short *data,
				    unsigned short count);
extern void ref_FPM_SMC_encoder(struct fpm_smc *smc, struct fpm_smc_ring *ring,
				const unsigned short *data,
				unsigned short count);
extern short ref_V22_PPS_filter(struct v22_pps *state,
				struct fpm_smc_ring *src, short *out,
				unsigned short count);
extern void ref_V22_PPS_init(struct v22_pps *state,
			     const struct v22_pps_cfg *cfg, int fresh);

extern void ref_FPM_AGC_init(struct fpm_agc *agc, const struct fpm_agc_cfg *cfg,
			     int reset);
extern void ref_FPM_AGC_agc(struct fpm_agc *agc, short *samples,
			    unsigned short count);
extern struct fpm_mtd *ref_FPM_MTD_create(struct fpm_mtd *state,
					  const struct fpm_mtd_cfg *cfg);
extern short ref_FPM_MTD_detect(struct fpm_mtd *state, const short *samples,
				short count);

extern unsigned int ref_dsplibs_debug_level;

extern const struct fpm_smc_cfg ref_SMCv22_CFG;
extern const struct fpm_agc_cfg ref_AGCv22_CFG2;
extern const struct fpm_mtd_cfg ref_MTDv22_CFG;
extern const struct fpm_mtd_cfg ref_MTDv22_CFG2;
extern const struct v22_pps_cfg ref_PPSv22_CFG;
extern const short ref_PPSv22_COFFS[];
extern const short ref_SMCv22_IMAP_1200BPS[];
extern const short ref_SMCv22_QMAP_1200BPS[];

/* Past V22FP_SDM_RX (0x1cc) plus a whole fpm_sdm. */
#define FP_SIZE		0x200
#define OBJ_SIZE	0x60

/* --------------------------------------------------------------------- */

/*
 * One xorshift, used for every fixture.  Both sides are built from the same
 * seed, so the pseudorandom filler is identical in the two buffers and a
 * whole-block comparison stays a comparison.
 */
static unsigned rng_state;

static void
rng_seed(unsigned s)
{
	rng_state = s ? s : 1u;
}

static unsigned
rng_next(void)
{
	rng_state ^= (rng_state << 13) & 0xffffffffu;
	rng_state ^= rng_state >> 17;
	rng_state ^= (rng_state << 5) & 0xffffffffu;
	return rng_state & 0xffffffffu;
}

/*
 * The instance block holds the pointer to the FP block, which is each side's
 * OWN buffer and so differs by construction.  Everything else in it must be
 * untouched, and the pointer itself is checked separately.
 */
static int
obj_first_diff(const unsigned char *b, const unsigned char *a)
{
	int i;

	for (i = 0; i < OBJ_SIZE; i++) {
		if (i >= V22_OBJ_GTIMER
		    && i < V22_OBJ_FP + (int)sizeof(void *))
			continue;	/* the two instance pointers */
		if (b[i] != a[i])
			return i;
	}
	return -1;
}

/* --------------------------------------------------------------------- */
/* ScrambleDataV22 and DescrambleDataV22                                 */

#define SDATA		64
#define SGUARD		4
#define SMARK		0xbeef

/*
 * `obj` FIRST and the alignment member last: the instance pointer handed to
 * the functions under test is `f->obj`, and a leading padding member would
 * put every offset in this file four or eight bytes out.
 */
struct sfix {
	unsigned char	obj[OBJ_SIZE];
	unsigned char	fp[FP_SIZE];
	double		align;
};

static struct sfix sa, sb;

/*
 * Lay an `fpm_sdm` down by hand rather than through `FPM_SDM_init`, so the
 * shift register can be seeded: init clears it, and a zero register makes the
 * first words of a scramble depend on nothing but the data, which is exactly
 * where a wrong-object reading would agree with the right one.
 */
static void
sdm_put(unsigned char *fp, int off, short nbits, short tap1, short tap2,
	unsigned int reg, short pad06)
{
	struct fpm_sdm s;

	memset(&s, 0, sizeof(s));
	s.cfg.nbits = nbits;
	s.cfg.tap1 = tap1;
	s.cfg.tap2 = tap2;
	s.pad06 = pad06;
	s.mask = (1 << nbits) - 1;
	s.notmask = ~s.mask;
	s.reg = reg;
	s.shift1 = (short)(tap1 - nbits);
	s.shift2 = (short)(tap2 - nbits);
	memcpy(fp + off, &s, sizeof(s));
}

/*
 * The two objects are deliberately UNLIKE each other: 4 bits per word against
 * 2 (which is the pair V22FP_create really builds, one per connection rate,
 * though never both at once -- a fixture choice, not evidence), and two
 * different registers.  SDMv22_CFG's taps, 14 and 17, are kept for both.
 */
static void
sdm_fixture(struct sfix *f, unsigned s, unsigned int reg_tx,
	    unsigned int reg_rx)
{
	int i;

	rng_seed(s);
	for (i = 0; i < FP_SIZE; i++)
		f->fp[i] = (unsigned char)rng_next();
	memset(f->obj, 0, sizeof(f->obj));
	*(void **)(void *)(f->obj + V22_OBJ_FP) = f->fp;

	sdm_put(f->fp, V22FP_SDM_TX, 4, 14, 17, reg_tx, 0x1111);
	sdm_put(f->fp, V22FP_SDM_RX, 2, 14, 17, reg_rx, 0x2222);
}

static int scram_off_sep, scram_swap_sep;
static int descram_off_sep, descram_swap_sep;

/*
 * `which` 0 drives ScrambleDataV22, 1 DescrambleDataV22.  The two are the
 * same shape, so they share a driver and the wrong readings are the mirror of
 * one another.
 */
static void
run_sdm_one(int which, int n, unsigned s, unsigned int reg_tx,
	    unsigned int reg_rx)
{
	unsigned short src[SDATA + SGUARD];
	unsigned short da[SDATA + SGUARD], db[SDATA + SGUARD];
	unsigned short alt[SDATA + SGUARD];
	struct fpm_sdm scratch;
	int right_off = which ? V22FP_SDM_RX : V22FP_SDM_TX;
	int wrong_off = which ? V22FP_SDM_TX : V22FP_SDM_RX;
	long where = (long)(which * 1000 + n);
	int i;

	sdm_fixture(&sa, s, reg_tx, reg_rx);
	sdm_fixture(&sb, s, reg_tx, reg_rx);

	for (i = 0; i < SDATA + SGUARD; i++)
		src[i] = (unsigned short)rng_next();
	for (i = n; i < n + SGUARD; i++)
		src[i] = SMARK;
	memcpy(da, src, sizeof(src));
	memcpy(db, src, sizeof(src));

	if (which) {
		ref_DescrambleDataV22(sa.obj, da, (unsigned short)n);
		DescrambleDataV22(sb.obj, db, (unsigned short)n);
	} else {
		ref_ScrambleDataV22(sa.obj, da, (unsigned short)n);
		ScrambleDataV22(sb.obj, db, (unsigned short)n);
	}

	for (i = 0; i < n + SGUARD; i++)
		diff_eq_int("word %ld", db[i], da[i], i);
	for (i = n; i < n + SGUARD; i++)
		diff_eq_int("guard word %ld untouched", da[i], SMARK, i);
	diff_eq_int("at %ld: FP block", memcmp(sb.fp, sa.fp, FP_SIZE), 0,
		    where);
	diff_eq_int("at %ld: first differing instance byte",
		    obj_first_diff(sb.obj, sa.obj), -1, where);
	diff_eq_int("at %ld: FP pointer still ours",
		    *(void **)(void *)(sb.obj + V22_OBJ_FP) == (void *)sb.fp,
		    1, where);

	/*
	 * WRONG READING 1: the other offset.  Run the same entry point over
	 * the object at the offset this function does NOT use.
	 */
	memcpy(&scratch, sb.fp + wrong_off, sizeof(scratch));
	memcpy(alt, src, sizeof(src));
	if (which)
		ref_FPM_SDM_descrambler(&scratch, alt, (unsigned short)n);
	else
		ref_FPM_SDM_scrambler(&scratch, alt, (unsigned short)n);
	if (memcmp(alt, da, (size_t)n * sizeof(alt[0])) != 0) {
		if (which)
			descram_off_sep++;
		else
			scram_off_sep++;
	}

	/*
	 * WRONG READING 2: the other entry point on the RIGHT object.  The
	 * scrambler and the descrambler are feed-back and feed-forward forms
	 * of one recurrence, so they agree on the first word out of a cleared
	 * register and diverge afterwards -- which is why the registers are
	 * seeded.
	 */
	memcpy(&scratch, sb.fp + right_off, sizeof(scratch));
	memcpy(alt, src, sizeof(src));
	if (which)
		ref_FPM_SDM_scrambler(&scratch, alt, (unsigned short)n);
	else
		ref_FPM_SDM_descrambler(&scratch, alt, (unsigned short)n);
	if (memcmp(alt, da, (size_t)n * sizeof(alt[0])) != 0
	    || scratch.reg != *(unsigned int *)(void *)(sa.fp + right_off
						       + 0x10)) {
		if (which)
			descram_swap_sep++;
		else
			scram_swap_sep++;
	}
}

static int
run_sdm(void)
{
	static const int counts[] = { 0, 1, 2, 3, 5, 8, 16, 31, 64 };
	static const unsigned seeds[] = {
		0x13572468u, 0x0f0f0f0fu, 0x7fffffffu, 0x00000001u
	};
	int rc, c, s, which;

	diff_begin("ScrambleDataV22 / DescrambleDataV22");
	for (which = 0; which < 2; which++)
		for (s = 0; s < (int)(sizeof(seeds) / sizeof(seeds[0])); s++)
			for (c = 0;
			     c < (int)(sizeof(counts) / sizeof(counts[0]));
			     c++)
				run_sdm_one(which, counts[c], seeds[s],
					    seeds[s] ^ 0x2a5a5a5au,
					    seeds[s] ^ 0x00ff00ffu);
	rc = diff_end();
	return rc;
}

/* --------------------------------------------------------------------- */
/* ModDataV22                                                            */

#define RING_LEN	64
#define NDATA		2600
#define NOUT_BIG	36864
#define NOUT_SMALL	4096
#define OMARK		0x5ead
/* A valid constellation index -- imap and qmap have sixteen entries. */
#define RING_FILL	5

struct modfix {
	unsigned char	obj[OBJ_SIZE];
	unsigned char	fp[FP_SIZE];
	short		sym[RING_LEN];
	short		coeff_i[V22_PPS_COEFFS];
	short		coeff_q[V22_PPS_COEFFS];
	short		hist_i[V22_PPS_HISTORY];
	short		hist_q[V22_PPS_HISTORY];
	double		align;		/* see the note on struct sfix */
};

static struct modfix ma, mb, mc;
static unsigned short mdata[NDATA];
static short oa[NOUT_BIG], ob[NOUT_BIG], oc[NOUT_SMALL];
static unsigned char fp_pre[FP_SIZE];
static unsigned char fp_skip[FP_SIZE];

static struct fpm_smc *
smc_of(struct modfix *f)
{
	return (struct fpm_smc *)(void *)(f->fp + V22FP_SMC);
}

static struct fpm_smc_ring *
ring_of(struct modfix *f)
{
	return (struct fpm_smc_ring *)(void *)(f->fp + V22FP_SMC_RING);
}

static struct v22_pps *
pps_of(struct modfix *f)
{
	return (struct v22_pps *)(void *)(f->fp + V22FP_PPS);
}

static void
mod_fixture(struct modfix *f, unsigned s, int use_ref)
{
	struct v22_pps_cfg cfg;
	struct fpm_smc *smc;
	struct fpm_smc_ring *ring;
	struct v22_pps *pps;
	int i;

	rng_seed(s);
	for (i = 0; i < FP_SIZE; i++)
		f->fp[i] = (unsigned char)rng_next();
	memset(f->obj, 0, sizeof(f->obj));
	*(void **)(void *)(f->obj + V22_OBJ_FP) = f->fp;

	smc = smc_of(f);
	memset(smc, 0, sizeof(*smc));
	smc->cfg = ref_SMCv22_CFG;
	smc->quad = (short)(rng_next() & 0x0cu);
	smc->acc = (short)(rng_next() & 0x0fu);

	for (i = 0; i < RING_LEN; i++)
		f->sym[i] = (short)(rng_next() & 0x0fu);
	ring = ring_of(f);
	memset(ring, 0, sizeof(*ring));
	ring->sym = f->sym;
	ring->widx = 0;
	ring->ridx = 0;
	ring->len = RING_LEN;

	for (i = 0; i < V22_PPS_COEFFS; i++) {
		f->coeff_i[i] = ref_PPSv22_COFFS[i];
		f->coeff_q[i] = ref_PPSv22_COFFS[(i + V22_PPS_PHASES / 4 * 3)
						 % V22_PPS_COEFFS];
	}
	memset(f->hist_i, 0, sizeof(f->hist_i));
	memset(f->hist_q, 0, sizeof(f->hist_q));

	pps = pps_of(f);
	memset(pps, 0, sizeof(*pps));
	pps->hist_i = f->hist_i;
	pps->hist_q = f->hist_q;
	cfg = ref_PPSv22_CFG;
	cfg.coeff_i = f->coeff_i;
	cfg.coeff_q = f->coeff_q;
	if (use_ref)
		ref_V22_PPS_init(pps, &cfg, 0);
	else
		V22_PPS_init(pps, &cfg, 0);
	/* Init does not set these; V22FP_create writes them directly. */
	pps->imap = ref_SMCv22_IMAP_1200BPS;
	pps->qmap = ref_SMCv22_QMAP_1200BPS;
}

/*
 * The FP block holds five pointers into the fixture's OWN arrays, so those
 * bytes differ between the two sides by construction and are excluded from
 * the block comparison.  Derived from the struct types rather than written
 * out as numbers, so a retype cannot leave a stale offset behind.
 */
static void
mark_ptr(const struct modfix *f, const void *p)
{
	int off = (int)((const unsigned char *)p - f->fp);
	int i;

	for (i = 0; i < (int)sizeof(void *); i++)
		fp_skip[off + i] = 1;
}

static void
fp_skip_init(struct modfix *f)
{
	memset(fp_skip, 0, sizeof(fp_skip));
	mark_ptr(f, &pps_of(f)->cfg.coeff_i);
	mark_ptr(f, &pps_of(f)->cfg.coeff_q);
	mark_ptr(f, &pps_of(f)->hist_i);
	mark_ptr(f, &pps_of(f)->hist_q);
	mark_ptr(f, &ring_of(f)->sym);
}

static int
fp_first_diff(const struct modfix *b, const struct modfix *a)
{
	int i;

	for (i = 0; i < FP_SIZE; i++)
		if (!fp_skip[i] && b->fp[i] != a->fp[i])
			return i;
	return -1;
}

static int changed(const unsigned char *now, int off, int len)
{
	return memcmp(now + off, fp_pre + off, (size_t)len) != 0;
}

static int mod_ring_sep, mod_ret_high, mod_smc_written, mod_pps_written;
static int mod_ring_written;

static void
run_mod_one(int n, unsigned s, int calls, short *bufa, short *bufb, int nout,
	    int with_alt)
{
	static struct fpm_smc_ring alt_ring;
	static short alt_sym[RING_LEN];
	unsigned short ra, rb;
	int call, i;
	long where;

	mod_fixture(&ma, s, 1);
	mod_fixture(&mb, s, 0);
	if (with_alt)
		mod_fixture(&mc, s, 1);
	fp_skip_init(&ma);

	rng_seed(s ^ 0x5a5a5a5au);
	for (i = 0; i < NDATA; i++)
		mdata[i] = (unsigned short)rng_next();

	for (call = 0; call < calls; call++) {
		where = (long)(n * 10 + call);

		for (i = 0; i < nout; i++)
			bufa[i] = bufb[i] = (short)OMARK;
		memcpy(fp_pre, ma.fp, FP_SIZE);

		ra = ref_ModDataV22(ma.obj, mdata, bufa, (unsigned short)n);
		rb = ModDataV22(mb.obj, mdata, bufb, (unsigned short)n);

		diff_eq_int("at %ld: samples returned", (long)rb, (long)ra,
			    where);
		diff_eq_int("at %ld: return fits the buffer", ra < nout, 1,
			    where);
		if (ra >= (unsigned short)nout)
			return;			/* refuse to read past it */

		for (i = 0; i < (int)ra; i++)
			diff_eq_int("sample %ld", bufb[i], bufa[i], i);
		diff_eq_int("at %ld: nothing past the returned count",
			    bufa[ra] == (short)OMARK, 1, where);

		diff_eq_int("at %ld: first differing FP byte",
			    fp_first_diff(&mb, &ma), -1, where);
		for (i = 0; i < RING_LEN; i++)
			diff_eq_int("ring symbol %ld", mb.sym[i], ma.sym[i], i);
		for (i = 0; i < V22_PPS_HISTORY; i++) {
			diff_eq_int("hist_i[%ld]", mb.hist_i[i], ma.hist_i[i],
				    i);
			diff_eq_int("hist_q[%ld]", mb.hist_q[i], ma.hist_q[i],
				    i);
		}
		diff_eq_int("at %ld: first differing instance byte",
			    obj_first_diff(mb.obj, ma.obj), -1, where);
		diff_eq_int("at %ld: FP pointer still ours",
			    *(void **)(void *)(mb.obj + V22_OBJ_FP)
			    == (void *)mb.fp, 1, where);

		if (ra >= 0x8000u)
			mod_ret_high++;
		if (changed(ma.fp, V22FP_SMC, (int)sizeof(struct fpm_smc)))
			mod_smc_written++;
		if (changed(ma.fp, V22FP_PPS, (int)sizeof(struct v22_pps)))
			mod_pps_written++;
		if (changed(ma.fp, V22FP_SMC_RING,
			    (int)sizeof(struct fpm_smc_ring)))
			mod_ring_written++;

		/*
		 * WRONG READING: the ring is not shared.  Encode into fixture
		 * c's own ring, then filter from a ring that nothing has
		 * written -- same cursors, same length, symbols the encoder
		 * did not produce.
		 */
		if (with_alt) {
			short rc_alt;

			for (i = 0; i < RING_LEN; i++)
				alt_sym[i] = RING_FILL;
			alt_ring = *ring_of(&mc);
			alt_ring.sym = alt_sym;

			for (i = 0; i < nout; i++)
				oc[i] = (short)OMARK;
			ref_FPM_SMC_encoder(smc_of(&mc), ring_of(&mc), mdata,
					    (unsigned short)n);
			rc_alt = ref_V22_PPS_filter(pps_of(&mc), &alt_ring, oc,
						    (unsigned short)n);
			if ((unsigned short)rc_alt != ra
			    || memcmp(oc, bufa,
				      (size_t)ra * sizeof(oc[0])) != 0)
				mod_ring_sep++;
		}
	}
}

static int
run_mod(void)
{
	static const int counts[] = { 0, 1, 2, 3, 7, 13, 40, 64, 97, 200 };
	static const unsigned seeds[] = { 0x2468ace0u, 0x11223344u };
	int rc, c, s;

	diff_begin("ModDataV22");
	for (s = 0; s < (int)(sizeof(seeds) / sizeof(seeds[0])); s++)
		for (c = 0; c < (int)(sizeof(counts) / sizeof(counts[0])); c++)
			run_mod_one(counts[c], seeds[s], 3, oa, ob, NOUT_SMALL,
				    1);
	rc = diff_end();

	/*
	 * And one call long enough that the sample count sets bit 15.  Forty
	 * outputs every three symbols, so 2,458 symbols is the threshold;
	 * 2,600 clears it with room to spare and stays inside NOUT_BIG.
	 */
	diff_begin("ModDataV22, a count that overflows a signed short");
	run_mod_one(NDATA, 0x0badf00du, 1, oa, ob, NOUT_BIG, 0);
	rc |= diff_end();

	return rc;
}

/* --------------------------------------------------------------------- */
/* Detect_v22                                                            */

#define SHR_SIZE	0x40
#define DGUARD		8
#define DMARK		0x3c3c
#define DBLOCK		V22_DETECT_BLOCK
#define DACC		16

struct dfix {
	unsigned char	obj[OBJ_SIZE];
	unsigned char	fp[FP_SIZE];
	unsigned char	shr[SHR_SIZE];
	struct fpm_mtd	mtd_a;
	struct fpm_mtd	mtd_b;
	short		acc_a[DACC];
	short		acc_b[DACC];
	double		align;		/* see the note on struct sfix */
};

/*
 * Six of everything: the blob, ours, and four model drivers -- one faithful,
 * three carrying a named wrong reading.  Each model needs its own detector
 * state, because a variant that clears a sub-block the original kept feeds
 * different samples into the next call's detectors, and the divergence has to
 * be allowed to run.
 */
#define NVARIANT	4

static struct dfix dfa, dfb, dfv[NVARIANT];
static short dsig[DBLOCK + DGUARD];
static short dbuf[2 + NVARIANT][DBLOCK + DGUARD];
static unsigned char dfp_pre[FP_SIZE];

/*
 * WHICH CONFIG GOES TO +0x18 AND WHICH TO +0x20 IS A FIXTURE CHOICE, NOT
 * EVIDENCE: the shared block is built by something not reconstructed.  What
 * matters is that the two detectors are DIFFERENT -- a pairing swap is
 * invisible on every sub-block where they agree -- so `MTDv22_CFG` and
 * `MTDv22_CFG2` rather than one config twice, and the number of sub-blocks on
 * which they actually disagreed is asserted non-zero at the end.
 */
static void
det_fixture(struct dfix *f, unsigned s, int use_ref)
{
	struct fpm_agc *agc;
	int i;

	rng_seed(s);
	for (i = 0; i < FP_SIZE; i++)
		f->fp[i] = (unsigned char)rng_next();
	memset(f->obj, 0, sizeof(f->obj));
	memset(f->shr, 0, sizeof(f->shr));
	memset(&f->mtd_a, 0, sizeof(f->mtd_a));
	memset(&f->mtd_b, 0, sizeof(f->mtd_b));
	memset(f->acc_a, 0, sizeof(f->acc_a));
	memset(f->acc_b, 0, sizeof(f->acc_b));

	*(void **)(void *)(f->obj + V22_OBJ_FP) = f->fp;
	*(void **)(void *)(f->obj + V22_OBJ_GTIMER) = f->shr;

	/* A caller-supplied state means caller-supplied accumulators. */
	f->mtd_a.acc = f->acc_a;
	f->mtd_b.acc = f->acc_b;
	if (use_ref) {
		ref_FPM_MTD_create(&f->mtd_a, &ref_MTDv22_CFG);
		ref_FPM_MTD_create(&f->mtd_b, &ref_MTDv22_CFG2);
	} else {
		FPM_MTD_create(&f->mtd_a, &ref_MTDv22_CFG);
		FPM_MTD_create(&f->mtd_b, &ref_MTDv22_CFG2);
	}
	*(void **)(void *)(f->shr + V22SHR_MTD_A) = &f->mtd_a;
	*(void **)(void *)(f->shr + V22SHR_MTD_B) = &f->mtd_b;

	agc = (struct fpm_agc *)(void *)(f->fp + V22FP_DET_AGC);
	memset(agc, 0, sizeof(*agc));
	if (use_ref)
		ref_FPM_AGC_init(agc, &ref_AGCv22_CFG2, 1);
	else
		FPM_AGC_init(agc, &ref_AGCv22_CFG2, 1);
}

/*
 * The signal: eight (frequency, amplitude) pairs, one selected per sub-block.
 *
 * THE TWO FREQUENCIES ARE MEASURED, NOT DERIVED.  Sweeping 100..3900 Hz in
 * 100 Hz steps through this exact fixture at four amplitudes, `MTDv22_CFG`'s
 * detector answers FPM_MTD_ABSENT at 2200 Hz and NOWHERE ELSE, and
 * `MTDv22_CFG2`'s at 1800 Hz and nowhere else; below about 200 Hz both fall
 * under `min_level` and answer FPM_MTD_NOSIGNAL.  Since a zero verdict is
 * what advances a counter, ONLY those two frequencies can move one at all --
 * a table of plausible V.22 tones (600, 1200, 2100, 2400, 3000) leaves every
 * counter pinned at zero, every sub-block cleared, and five separating counts
 * dead.  Nothing here is a claim about what either detector is FOR; the
 * coefficient banks belong to an object nothing reconstructed builds.
 *
 * Silence and 100 Hz are carried because NOSIGNAL is a THIRD verdict and is
 * non-zero like a detection, so it RESETS a run rather than advancing it.
 */
static const struct { int hz; int amp; } dtones[] = {
	{ 2200, 12000 },	/* A absent, B present  */
	{ 1800, 12000 },	/* A present, B absent  */
	{ 1200, 12000 },	/* both present         */
	{ 2400, 12000 },	/* both present         */
	{  100, 12000 },	/* below the gate: both NOSIGNAL, and that is
				 *   NON-ZERO, so it resets a run           */
	{    0,	    0 },	/* silence                                  */
	{ 2200,   750 },	/* the same two, well down in level         */
	{ 1800,   750 }
};
#define NTONE	(int)(sizeof(dtones) / sizeof(dtones[0]))

static double dphase;

static void
make_block(const int *sel)
{
	int k, i;

	for (k = 0; k < V22_DETECT_SUBBLOCKS; k++) {
		int hz = dtones[sel[k]].hz;
		int amp = dtones[sel[k]].amp;

		for (i = 0; i < V22_DETECT_SUBBLOCK; i++) {
			dsig[k * V22_DETECT_SUBBLOCK + i] =
				(short)(amp * sin(dphase));
			dphase += 6.283185307179586 * hz / 8000.0;
		}
	}
	for (i = DBLOCK; i < DBLOCK + DGUARD; i++)
		dsig[i] = (short)DMARK;
}

struct model_out {
	int	printed;
	int	cleared;		/* sub-blocks zeroed, 0..4 */
	short	run_a, run_b;
	short	va[V22_DETECT_SUBBLOCKS];
	short	vb[V22_DETECT_SUBBLOCKS];
};

/*
 * The model.  `variant` 0 is faithful, and is checked against the blob on
 * every single call -- which is what licenses using it as the baseline the
 * other three are measured against.  Built out of the `ref_` callees so that
 * a variant's divergence is its own and not its callees'.
 *
 *   1  the counters paired with the other detector.  THIS COVERS TWO WRONG
 *      READINGS, not one: swapping which counter each verdict drives and
 *      swapping the +0x18 / +0x20 detector offsets are the same permutation,
 *      so a separating trial here retires both.
 *   2  the clear gated on the verdict alone, with no hold
 *   3  `>= 2` where the object has `> 2`
 */
static int
drive(struct dfix *f, short *data, int variant, struct model_out *o)
{
	short run_a = 0, run_b = 0;
	short thr = (variant == 3) ? (short)1 : (short)V22_DETECT_THRESHOLD;
	int i, j;

	o->cleared = 0;

	ref_FPM_AGC_agc((struct fpm_agc *)(void *)(f->fp + V22FP_DET_AGC),
			data, V22_DETECT_BLOCK);

	for (i = 0; i < V22_DETECT_SUBBLOCKS; i++) {
		short *chunk = data + i * V22_DETECT_SUBBLOCK;
		short va = ref_FPM_MTD_detect(&f->mtd_a, chunk,
					      V22_DETECT_SUBBLOCK);
		short vb = ref_FPM_MTD_detect(&f->mtd_b, chunk,
					      V22_DETECT_SUBBLOCK);
		short gate;
		int clear;

		if (vb != 0)
			run_b = 0;
		else
			run_b = (short)(run_b + 1);
		if (va != 0)
			run_a = 0;
		else
			run_a = (short)(run_a + 1);

		gate = (variant == 1) ? run_b : run_a;
		if (variant == 2)
			clear = (va != 0);
		else
			clear = (gate <= V22_DETECT_CLEAR_HOLD);

		if (clear) {
			for (j = 0; j < V22_DETECT_SUBBLOCK; j++)
				chunk[j] = 0;
			o->cleared++;
		}
		o->va[i] = va;
		o->vb[i] = vb;
	}

	o->run_a = run_a;
	o->run_b = run_b;
	o->printed = ((variant == 1) ? run_a : run_b) > thr;
	return (run_a > thr) | (run_b > thr);
}

static int det_sep[NVARIANT];
/*
 * The buffer channel on its own.  det_sep[] is an OR over three channels and
 * for variant 1 the RETURN channel is dead by construction, so det_sep[1]
 * could be satisfied entirely by the diagnostic -- which the differential
 * only compares on the trials that raise the level.  The claim being made is
 * that the CLEAR separates a swapped pairing, so that is what is counted.
 * Variants 2 and 3 have no independent claim here: 2 is a clear-gate change
 * and separates only through the buffer anyway, and 3 is thresholds alone,
 * whose buffer channel is empty by construction.
 */
static int det_sep_buf[NVARIANT];
static int det_disagreed;
static int det_true, det_false;
static int det_a_over, det_b_over;
static int det_cleared, det_kept, det_hold;
static int det_said_ok, det_agc_moved, det_mtd_moved;

static void
cmp_mtd(const char *f0, const char *f1, const char *f2, const char *f3,
	const struct fpm_mtd *b, const struct fpm_mtd *a, long where)
{
	diff_eq_int(f0, b->dc_state[0], a->dc_state[0], where);
	diff_eq_int(f1, b->dc_state[1], a->dc_state[1], where);
	diff_eq_int(f2, b->out_of_band, a->out_of_band, where);
	diff_eq_int(f3, b->wideband, a->wideband, where);
}

static void
run_detect_one(const int *pat0, const int *pat1, unsigned s, int debug_on,
	       long trial)
{
	struct model_out mo[NVARIANT];
	int ra, rb, rv, buf_differs;
	int call, v, i;
	long where;

	det_fixture(&dfa, s, 1);
	det_fixture(&dfb, s, 0);
	for (v = 0; v < NVARIANT; v++)
		det_fixture(&dfv[v], s, 1);

	dphase = 0.0;
	dsplibs_debug_level = ref_dsplibs_debug_level = debug_on ? 2u : 0u;

	for (call = 0; call < 4; call++) {
		const int *sel = (call & 1) ? pat1 : pat0;

		where = trial * 10 + call;
		make_block(sel);
		for (v = 0; v < 2 + NVARIANT; v++)
			memcpy(dbuf[v], dsig, sizeof(dsig));
		memcpy(dfp_pre, dfa.fp, FP_SIZE);

		dsplib_debug_capture_reset();
		ra = ref_Detect_v22(dfa.obj, dbuf[0]);
		rb = Detect_v22(dfb.obj, dbuf[1]);

		diff_eq_int("at %ld: verdict", rb, ra, where);
		for (i = 0; i < DBLOCK + DGUARD; i++)
			diff_eq_int("sample %ld", dbuf[1][i], dbuf[0][i], i);
		for (i = DBLOCK; i < DBLOCK + DGUARD; i++)
			diff_eq_int("guard sample %ld untouched", dbuf[0][i],
				    (short)DMARK, i);
		diff_eq_int("at %ld: first differing instance byte",
			    obj_first_diff(dfb.obj, dfa.obj), -1, where);
		/*
		 * The WHOLE FP block, not just the AGC: nothing in it points
		 * into the fixture (both detectors live outside it and both
		 * sides copy the same AGCv22_CFG2), so a plain comparison
		 * works and catches a stray write or a wrong V22FP_DET_AGC.
		 */
		diff_eq_int("at %ld: FP block",
			    memcmp(dfb.fp, dfa.fp, FP_SIZE), 0, where);
		cmp_mtd("at %ld: A dc_state[0]", "at %ld: A dc_state[1]",
			"at %ld: A out_of_band", "at %ld: A wideband",
			&dfb.mtd_a, &dfa.mtd_a, where);
		cmp_mtd("at %ld: B dc_state[0]", "at %ld: B dc_state[1]",
			"at %ld: B out_of_band", "at %ld: B wideband",
			&dfb.mtd_b, &dfa.mtd_b, where);
		for (i = 0; i < DACC; i++) {
			diff_eq_int("acc_a[%ld]", dfb.acc_a[i], dfa.acc_a[i],
				    i);
			diff_eq_int("acc_b[%ld]", dfb.acc_b[i], dfa.acc_b[i],
				    i);
		}
		diff_eq_int("at %ld: debug transcript",
			    strcmp(dsplib_debug_capture_text(0),
				   dsplib_debug_capture_text(1)), 0, where);

		/*
		 * On CONTENT, not on the line count: FPM_AGC_init and
		 * FPM_AGC_Freeze/Release have diagnostic sites of their own,
		 * so a non-empty buffer does not mean THIS function's arm
		 * fired.  Finding 149's trap, and harness.h names it.
		 */
		if (strstr(dsplib_debug_capture_text(1), "Detect_V22 OK")
		    != NULL)
			det_said_ok++;

		/* The faithful model first, then the three wrong readings. */
		for (v = 0; v < NVARIANT; v++) {
			rv = drive(&dfv[v], dbuf[2 + v], v, &mo[v]);
			if (v == 0) {
				diff_eq_int("at %ld: model verdict", rv, ra,
					    where);
				for (i = 0; i < DBLOCK; i++)
					diff_eq_int("model sample %ld",
						    dbuf[2][i], dbuf[0][i], i);
				if (debug_on)
					diff_eq_int("at %ld: model printing",
						    mo[0].printed,
						    strstr(
						    dsplib_debug_capture_text(1),
						    "Detect_V22 OK") != NULL,
						    where);
				continue;
			}
			buf_differs = memcmp(dbuf[2 + v], dbuf[0],
					     DBLOCK * sizeof(short)) != 0;
			if (rv != ra || mo[v].printed != mo[0].printed
			    || buf_differs)
				det_sep[v]++;
			if (buf_differs)
				det_sep_buf[v]++;
		}

		if (ra)
			det_true++;
		else
			det_false++;
		if (mo[0].run_a > V22_DETECT_THRESHOLD)
			det_a_over++;
		if (mo[0].run_b > V22_DETECT_THRESHOLD)
			det_b_over++;
		det_cleared += mo[0].cleared;
		det_kept += V22_DETECT_SUBBLOCKS - mo[0].cleared;
		for (i = 0; i < V22_DETECT_SUBBLOCKS; i++)
			if (mo[0].va[i] != mo[0].vb[i])
				det_disagreed++;
		if (memcmp(dfa.fp + V22FP_DET_AGC, dfp_pre + V22FP_DET_AGC,
			   sizeof(struct fpm_agc)) != 0)
			det_agc_moved++;
		if (dfa.mtd_a.wideband != 0 || dfa.mtd_b.wideband != 0)
			det_mtd_moved++;
		/*
		 * The case the CLEAR HOLD exists for: a sub-block the first
		 * detector was quiet on, cleared anyway because it is the
		 * first of a run.  Variant 2 is exactly the reading that gets
		 * this wrong, so it has to happen.
		 */
		{
			short run = 0;

			for (i = 0; i < V22_DETECT_SUBBLOCKS; i++) {
				if (mo[0].va[i] != 0)
					run = 0;
				else if (++run == 1)
					det_hold++;
			}
		}
	}

	dsplibs_debug_level = ref_dsplibs_debug_level = 0u;
}

static int
run_detect(void)
{
	int pat0[V22_DETECT_SUBBLOCKS], pat1[V22_DETECT_SUBBLOCKS];
	int rc, t0, t1;
	long trial = 0;

	/*
	 * The accumulator arrays are the caller's, so they have to be big
	 * enough for whatever the two configs ask for.  Checked rather than
	 * assumed: an overflow of acc_a into acc_b would present as a
	 * mysterious detector divergence a long way from its cause.
	 */
	diff_begin("Detect_v22 fixture is big enough");
	diff_eq_int("MTDv22_CFG tones fit (%ld)",
		    ref_MTDv22_CFG.tones * 2 <= DACC, 1,
		    ref_MTDv22_CFG.tones);
	diff_eq_int("MTDv22_CFG2 tones fit (%ld)",
		    ref_MTDv22_CFG2.tones * 2 <= DACC, 1,
		    ref_MTDv22_CFG2.tones);
	rc = diff_end();

	diff_begin("Detect_v22");
	for (t0 = 0; t0 < NTONE; t0++)
		for (t1 = 0; t1 < NTONE; t1++) {
			pat0[0] = t0; pat0[1] = t0; pat0[2] = t1; pat0[3] = t1;
			pat1[0] = t0; pat1[1] = t1; pat1[2] = t1; pat1[3] = t1;
			run_detect_one(pat0, pat1,
				       0x51ed0001u + (unsigned)(t0 * 16 + t1),
				       ((t0 + t1) & 1) != 0, trial++);
		}

	/*
	 * And the patterns that land a counter on EXACTLY two, which is the
	 * only place `> 2` and `>= 2` can be told apart: one noisy sub-block
	 * followed by two quiet ones, and its mirror.
	 */
	for (t0 = 0; t0 < NTONE; t0++)
		for (t1 = 0; t1 < NTONE; t1++) {
			pat0[0] = t0; pat0[1] = t0; pat0[2] = t1; pat0[3] = t1;
			pat1[0] = t1; pat1[1] = t1; pat1[2] = t0; pat1[3] = t0;
			run_detect_one(pat0, pat1,
				       0x6a1c0001u + (unsigned)(t0 * 16 + t1),
				       ((t0 * t1) & 1) != 0, trial++);
		}
	rc |= diff_end();
	return rc;
}

/* --------------------------------------------------------------------- */

int
main(void)
{
	int rc = 0;

	dsplib_debug_capture_on = 1;

	rc |= run_sdm();
	rc |= run_mod();
	rc |= run_detect();

	/*
	 * The separating counts.  Every one of these is the number of trials
	 * on which a NAMED wrong reading produced a different answer from the
	 * blob's.  A zero here would mean the corresponding check above is
	 * decoration: it would pass just as happily against the wrong object,
	 * the wrong entry point or an unshared ring.
	 */
	diff_begin("v22data separating trials");
	diff_eq_int("scrambler at the wrong offset separates (%ld)",
		    scram_off_sep > 0, 1, scram_off_sep);
	diff_eq_int("scrambler against descrambler separates (%ld)",
		    scram_swap_sep > 0, 1, scram_swap_sep);
	diff_eq_int("descrambler at the wrong offset separates (%ld)",
		    descram_off_sep > 0, 1, descram_off_sep);
	diff_eq_int("descrambler against scrambler separates (%ld)",
		    descram_swap_sep > 0, 1, descram_swap_sep);
	diff_eq_int("an unshared ring separates (%ld)", mod_ring_sep > 0, 1,
		    mod_ring_sep);
	diff_eq_int("a sample count with bit 15 set was reached (%ld)",
		    mod_ret_high > 0, 1, mod_ret_high);
	diff_eq_int("the symbol coder at 0x48 was written (%ld)",
		    mod_smc_written > 0, 1, mod_smc_written);
	diff_eq_int("the pulse shaper at 0x78 was written (%ld)",
		    mod_pps_written > 0, 1, mod_pps_written);
	diff_eq_int("the ring at 0xa0 was written (%ld)",
		    mod_ring_written > 0, 1, mod_ring_written);

	diff_eq_int("the detector pairing separates (%ld)", det_sep[1] > 0, 1,
		    det_sep[1]);
	diff_eq_int("...and separates in the SAMPLE BUFFER (%ld)",
		    det_sep_buf[1] > 0, 1, det_sep_buf[1]);
	diff_eq_int("the clear hold separates (%ld)", det_sep[2] > 0, 1,
		    det_sep[2]);
	diff_eq_int("the > 2 threshold separates >= 2 (%ld)", det_sep[3] > 0,
		    1, det_sep[3]);
	diff_eq_int("the two detectors ever disagreed (%ld)",
		    det_disagreed > 0, 1, det_disagreed);
	diff_eq_int("Detect_v22 answered true (%ld)", det_true > 0, 1,
		    det_true);
	diff_eq_int("Detect_v22 answered false (%ld)", det_false > 0, 1,
		    det_false);
	diff_eq_int("counter A passed the threshold (%ld)", det_a_over > 0, 1,
		    det_a_over);
	diff_eq_int("counter B passed the threshold (%ld)", det_b_over > 0, 1,
		    det_b_over);
	diff_eq_int("a sub-block was cleared (%ld)", det_cleared > 0, 1,
		    det_cleared);
	diff_eq_int("a sub-block was kept (%ld)", det_kept > 0, 1, det_kept);
	diff_eq_int("the clear hold was exercised (%ld)", det_hold > 0, 1,
		    det_hold);
	diff_eq_int("the blob printed Detect_V22 OK (%ld)", det_said_ok > 0, 1,
		    det_said_ok);
	diff_eq_int("the AGC state moved (%ld)", det_agc_moved > 0, 1,
		    det_agc_moved);
	diff_eq_int("the detector state moved (%ld)", det_mtd_moved > 0, 1,
		    det_mtd_moved);
	rc |= diff_end();

	return rc;
}

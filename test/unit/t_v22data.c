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

#include <string.h>

#include "harness.h"
#include "dsplib/v22data.h"
#include "dsplib/fpm_sdm.h"
#include "dsplib/fpm_smc.h"
#include "dsplib/v22_pps.h"

extern void ref_ScrambleDataV22(void *modem, unsigned short *data,
				unsigned short count);
extern void ref_DescrambleDataV22(void *modem, unsigned short *data,
				  unsigned short count);
extern unsigned short ref_ModDataV22(void *modem, const unsigned short *data,
				     short *out, unsigned short count);

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

extern const struct fpm_smc_cfg ref_SMCv22_CFG;
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

int
main(void)
{
	int rc = 0;

	rc |= run_sdm();
	rc |= run_mod();

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
	rc |= diff_end();

	return rc;
}

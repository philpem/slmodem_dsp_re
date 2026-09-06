/*
 * t_v21create.c -- differential test of `V21RX_create` (.text 0x098e70, 1,011
 *                  bytes) against the blob's own.
 *
 * The constructor builds a tree, not a struct: a 0x54-byte handle, a 0x10-byte
 * half-duplex context, a 0x94-byte DSP block, a 0x140-byte intermediate
 * buffer, an `fpm_mtd` with its own accumulator array, and the three buffers
 * `FPM_FSD_init` and `FPM_MRF_init` hang off their own configurations. Every
 * one of those is compared.
 *
 * POINTER SLOTS ARE SKIPPED AND THEIR TARGETS COMPARED BY CONTENT, which is
 * `t_b103create.c`'s method and the only claim a differential test can make
 * about an address: ours and the blob's allocate from the same
 * `sysdep_malloc` -- the object leaves it UNDEFINED, so there is one allocator
 * and not two -- but they allocate at different times and so at different
 * addresses. `handle_is_pointer`, `hdx_is_pointer` and `dsp_is_pointer` name
 * the slots, and every one of them is then followed to what it reaches.
 *
 * THE TWO SLOTS THAT ARE NOT SKIPPED BUT CHECKED AS A RELATION. `rx + 0x1c`
 * and `rx + 0x24` hold the FSD's trace buffer and the ADDRESS of the FSD's
 * `last_count`. Neither can be compared across the two sides, and both are a
 * pointer INTO this side's own tree -- so each is asserted equal to the thing
 * it should point at, on each side independently. A constructor that stored
 * the wrong field's address would pass a skip and fail this.
 *
 * D955/F8587: PLANT EVERY FIELD USED AS A SUBSCRIPT, NOT ONLY THOSE
 * DEREFERENCED. The caller-supplied path is entered with the handle set to a
 * NON-ZERO pattern everywhere except the two sub-pointers, so a field the
 * constructor forgets to write shows up as the pattern rather than as a zero
 * that a fresh `sysdep_malloc` might have supplied anyway. The two
 * sub-pointers must still be NULL, because they are what the object tests to
 * decide whether to allocate, and a non-NULL pattern there is a wild free.
 *
 * WHAT EACH CASE IS FOR.
 *
 *   - `params == NULL` takes `V21RX_CFG`, which is the channel-2 arm.
 *   - An explicit `chan2 == 0` table is the ONLY way to reach the channel-1
 *     arm: `V21RX_CFG` ships 1, so a test that only ever passed NULL would
 *     leave `V21RX_CHAN1_INTRP`, `V21_CHAN1_MTD_COEFF` and `fsd.delay = 5`
 *     completely uncovered while looking thorough.
 *   - A NON-ZERO `aux` is what exercises D1181. The object stores that
 *     pointer over `fpm_fsd_cfg`'s `f18`/`pad1a` with one 32-bit `mov`; ours
 *     writes the same four bytes as two shorts. The claim is that the stored
 *     bytes are identical, and this is where it is measured rather than
 *     asserted -- with `aux` set to a value whose two halves DIFFER, so a
 *     split that got the halves the wrong way round is caught.
 *   - `fresh` is 0 on the caller-supplied path and 1 on the self-allocating
 *     one, and it is the third argument to all three `*_init` calls, so both
 *     paths are run for every configuration.
 */

#include <stddef.h>
#include <stdio.h>
#include <string.h>

#include "harness.h"

#include "dsplib/faxcfg.h"
#include "dsplib/fpm_agc.h"
#include "dsplib/fpm_fsd.h"
#include "dsplib/fpm_mrf.h"
#include "dsplib/fpm_mtd.h"
#include "dsplib/sysdep.h"
#include "dsplib/v21cfg.h"
#include "dsplib/v21fax.h"

extern void *ref_V21RX_create(void *modem, const struct v21rx_cfg *params);
extern void ref_V21RX_delete(void *modem);
extern int ref_V21RX_control(void *modem, const struct v21rx_ctl *arg);
extern short ref_RxHdxStartV21(void *modem, short *in, short *out,
			       short *count);
extern struct v21rx_cfg ref_V21RX_CFG;

#define OFF(t, f)	((int)offsetof(t, f))

/* ------------------------------------------------------------------------- */

static int
handle_is_pointer(int off)
{
	return off == V21RX_OBJ_TRACE
	       || off == V21RX_OBJ_COUNT_AT
	       || off == V21RX_OBJ_HDX
	       || off == V21RX_OBJ_DSP;
}

static int
hdx_is_pointer(int off)
{
	return off == OFF(struct v21_rx_hdx, handler);
}

static int
dsp_is_pointer(int off)
{
	return off == OFF(struct v21_rx_dsp, agc)
			+ OFF(struct fpm_agc, cfg)
			+ OFF(struct fpm_agc_cfg, alpha)
	       || off == OFF(struct v21_rx_dsp, agc)
			+ OFF(struct fpm_agc, cfg)
			+ OFF(struct fpm_agc_cfg, beta)
	       || off == OFF(struct v21_rx_dsp, mrf)
			+ OFF(struct fpm_mrf, cfg)
			+ OFF(struct fpm_mrf_cfg, coeff)
	       || off == OFF(struct v21_rx_dsp, mrf)
			+ OFF(struct fpm_mrf, cfg)
			+ OFF(struct fpm_mrf_cfg, aux)
	       || off == OFF(struct v21_rx_dsp, mrf)
			+ OFF(struct fpm_mrf, history)
	       || off == OFF(struct v21_rx_dsp, fsd)
			+ OFF(struct fpm_fsd, cfg)
			+ OFF(struct fpm_fsd_cfg, fir)
	       || off == OFF(struct v21_rx_dsp, fsd)
			+ OFF(struct fpm_fsd, cfg)
			+ OFF(struct fpm_fsd_cfg, iir)
	       || off == OFF(struct v21_rx_dsp, fsd)
			+ OFF(struct fpm_fsd, trace)
	       || off == OFF(struct v21_rx_dsp, fsd)
			+ OFF(struct fpm_fsd, fir_hist)
	       || off == OFF(struct v21_rx_dsp, fsd)
			+ OFF(struct fpm_fsd, iir_hist)
	       || off == OFF(struct v21_rx_dsp, mtd)
	       || off == OFF(struct v21_rx_dsp, mag);
}

static int
mtd_is_pointer(int off)
{
	return off == OFF(struct fpm_mtd, cfg) + OFF(struct fpm_mtd_cfg, coeff)
	       || off == OFF(struct fpm_mtd, acc);
}

/*
 * Compare two blocks word by word, skipping both halves of every pointer.
 * Reporting per WORD rather than per byte keeps one wrong 32-bit field to two
 * lines instead of four, and the offset is in the message, so a failure names
 * the field without a second tool.
 */
static void
compare_block(const char *what, const void *ours, const void *ref, int size,
	      int (*is_ptr)(int), long tag)
{
	const unsigned char *a = (const unsigned char *)ours;
	const unsigned char *b = (const unsigned char *)ref;
	char buf[160];
	int i;

	for (i = 0; i + 1 < size; i += 2) {
		if (is_ptr(i) || is_ptr(i - 2))
			continue;
		snprintf(buf, sizeof(buf), "%.90s word 0x%02x (%%ld)", what, i);
		diff_eq_int(buf, *(const short *)(const void *)(a + i),
			    *(const short *)(const void *)(b + i), tag);
	}
}

static void
compare_shorts(const char *what, const short *ours, const short *ref, int n,
	       long tag)
{
	char buf[160];
	int i;

	if (ours == 0 || ref == 0) {
		snprintf(buf, sizeof(buf), "%.90s: both present (%%ld)", what);
		diff_eq_int(buf, ours != 0, ref != 0, tag);
		return;
	}
	snprintf(buf, sizeof(buf), "%.90s[%%ld]", what);
	for (i = 0; i < n; i++) {
		if (ours[i] != ref[i]) {
			diff_eq_int(buf, ours[i], ref[i], i);
			return;
		}
	}
	diff_eq_int(buf, 0, 0, n);
	(void)tag;
}

/* Read the pointer stored at `off` in a handle the test only knows as bytes. */
static const void *
ptr_at(const void *base, int off)
{
	const void *p;

	memcpy(&p, (const char *)base + off, sizeof p);
	return p;
}

/* Map a state function pointer to an identity both sides agree on. */
static int
fn_id(const void *p)
{
	if (p == 0)
		return -1;
	if (p == (const void *)RxHdxStartV21
	    || p == (const void *)&ref_RxHdxStartV21)
		return 0;
	return -2;
}

/* ------------------------------------------------------------------------- */

static void
compare_tree(const char *what, void *a, void *b, long tag)
{
	struct v21_rx_hdx *ha = V21RX_HDX(a), *hb = V21RX_HDX(b);
	struct v21_rx_dsp *da = V21RX_DSP(a), *db = V21RX_DSP(b);
	char buf[160];

	diff_eq_int("hdx allocated on both sides (%ld)",
		    ha != 0 && hb != 0, 1, tag);
	diff_eq_int("dsp allocated on both sides (%ld)",
		    da != 0 && db != 0, 1, tag);
	if (ha == 0 || hb == 0 || da == 0 || db == 0)
		return;

	compare_block(what, a, b, V21RX_OBJ_SIZE, handle_is_pointer, tag);
	compare_block(what, ha, hb, (int)sizeof(*ha), hdx_is_pointer, tag);
	compare_block(what, da, db, (int)sizeof(*da), dsp_is_pointer, tag);

	/* The one pointer in the handle's siblings that has an identity. */
	snprintf(buf, sizeof(buf), "%.90s hdx.handler is RxHdxStartV21 (%%ld)",
		 what);
	diff_eq_int(buf, fn_id((const void *)ha->handler),
		    fn_id((const void *)hb->handler), tag);
	diff_eq_int("hdx.handler is the START state on both sides (%ld)",
		    fn_id((const void *)ha->handler), 0, tag);

	/*
	 * The two trace slots, as a RELATION on each side rather than as a
	 * comparison across them.
	 */
	diff_eq_int("rx+0x1c is dsp->fsd.trace, ours (%ld)",
		    ptr_at(a, V21RX_OBJ_TRACE)
		    == (const void *)da->fsd.trace, 1, tag);
	diff_eq_int("rx+0x1c is dsp->fsd.trace, blob (%ld)",
		    ptr_at(b, V21RX_OBJ_TRACE)
		    == (const void *)db->fsd.trace, 1, tag);
	diff_eq_int("rx+0x24 is &dsp->fsd.last_count, ours (%ld)",
		    ptr_at(a, V21RX_OBJ_COUNT_AT)
		    == (const void *)&da->fsd.last_count, 1, tag);
	diff_eq_int("rx+0x24 is &dsp->fsd.last_count, blob (%ld)",
		    ptr_at(b, V21RX_OBJ_COUNT_AT)
		    == (const void *)&db->fsd.last_count, 1, tag);

	/* Everything the skipped pointers reach. */
	compare_shorts("dsp.agc.cfg.alpha", da->agc.cfg.alpha,
		       db->agc.cfg.alpha, 2, tag);
	compare_shorts("dsp.agc.cfg.beta", da->agc.cfg.beta,
		       db->agc.cfg.beta, 2, tag);
	compare_shorts("dsp.mrf.cfg.coeff", da->mrf.cfg.coeff,
		       db->mrf.cfg.coeff, 360, tag);
	compare_shorts("dsp.mrf.history", da->mrf.history, db->mrf.history,
		       40, tag);
	compare_shorts("dsp.fsd.cfg.fir", da->fsd.cfg.fir, db->fsd.cfg.fir,
		       15, tag);
	compare_shorts("dsp.fsd.cfg.iir", da->fsd.cfg.iir, db->fsd.cfg.iir,
		       15, tag);
	compare_shorts("dsp.fsd.trace", da->fsd.trace, db->fsd.trace, 160,
		       tag);
	compare_shorts("dsp.fsd.fir_hist", da->fsd.fir_hist, db->fsd.fir_hist,
		       15, tag);
	compare_shorts("dsp.fsd.iir_hist", da->fsd.iir_hist, db->fsd.iir_hist,
		       2 * 3, tag);
	compare_shorts("dsp.mag", da->mag, db->mag, V21RX_MAG_BYTES / 2, tag);

	diff_eq_int("dsp.mtd built on both sides (%ld)",
		    da->mtd != 0 && db->mtd != 0, 1, tag);
	if (da->mtd != 0 && db->mtd != 0) {
		compare_block(what, da->mtd, db->mtd,
			      (int)sizeof(*da->mtd), mtd_is_pointer, tag);
		compare_shorts("mtd.cfg.coeff", da->mtd->cfg.coeff,
			       db->mtd->cfg.coeff, 10, tag);
		compare_shorts("mtd.acc", da->mtd->acc, db->mtd->acc,
			       2 * 2, tag);
	}
}

/* ------------------------------------------------------------------------- */

static const struct {
	const char *name;
	int use_default;	/* pass params == NULL                      */
	short chan2;
	unsigned long aux;
} cases[] = {
	{ "params NULL (V21RX_CFG, channel 2)", 1, 1, 0 },
	{ "explicit channel 2",                 0, 1, 0 },
	{ "explicit channel 1",                 0, 0, 0 },
	{ "channel 1, aux with unequal halves", 0, 0, 0x1234abcdUL },
	{ "channel 2, aux with unequal halves", 0, 1, 0xfedc0011UL }
};

#define NCASES	((long)(sizeof(cases) / sizeof(cases[0])))

static void
build_cfg(struct v21rx_cfg *c, long k)
{
	*c = V21RX_CFG;
	c->chan2 = cases[k].chan2;
	c->aux = (void *)cases[k].aux;
}

static int
test_self_allocating(void)
{
	long k;

	diff_begin("V21RX_create, self-allocating");

	for (k = 0; k < NCASES; k++) {
		struct v21rx_cfg ca, cb;
		void *a, *b;

		build_cfg(&ca, k);
		build_cfg(&cb, k);

		if (cases[k].use_default) {
			b = ref_V21RX_create(0, 0);
			a = V21RX_create(0, 0);
		} else {
			b = ref_V21RX_create(0, &cb);
			a = V21RX_create(0, &ca);
		}

		diff_eq_int("both built (%ld)", a != 0 && b != 0, 1, k);
		if (a == 0 || b == 0)
			continue;

		compare_tree(cases[k].name, a, b, k);

		V21RX_delete(a);
		ref_V21RX_delete(b);
	}

	return diff_end();
}

/*
 * The caller-supplied path, which is RE-INITIALISATION IN PLACE and is the
 * only safe way to reach it -- plus D955's planting.
 *
 * THE OBVIOUS SHAPE OF THIS TEST CRASHES, AND THE CRASH IS THE OBJECT'S
 * CONTRACT RATHER THAN A BUG IN EITHER SIDE. `t_b103create.c` supplies a
 * ZEROED handle for its caller-supplied case, and the same thing here
 * segfaults on both sides: `fresh` is 1 only when `V21RX_create` allocated the
 * handle, so a caller-supplied one always gets `fresh == 0`, and
 * `FPM_FSD_init` at `fresh == 0` zeroes `fir_hist`, `iir_hist` and `trace`
 * WITHOUT allocating them. A handle whose `dsp` is NULL therefore gets a
 * freshly-zeroed DSP block and then three NULL dereferences. Finding F9354.
 *
 * So the handle handed back in is one this function built a moment earlier,
 * which is what the object actually supports and what a caller doing a
 * re-train would do.
 *
 * THE PLANTING GOES HERE INSTEAD (D955/F8587). After the first create, the
 * whole handle is overwritten with a non-zero pattern EXCEPT the two
 * sub-pointers -- which must survive, because they are what the constructor
 * tests to decide whether to allocate. Every field the second create is
 * supposed to rewrite is then visibly the pattern if it does not, rather than
 * invisibly a zero that a fresh allocation would have supplied anyway.
 */
static int
test_reinit(void)
{
	long k;

	diff_begin("V21RX_create, re-initialised in place");

	for (k = 0; k < NCASES; k++) {
		struct v21rx_cfg ca, cb;
		void *a, *b;
		struct v21_rx_dsp *da, *db;
		struct v21_rx_hdx *ha, *hb;
		short *maga, *magb, *tracea, *traceb;

		build_cfg(&ca, k);
		build_cfg(&cb, k);

		b = ref_V21RX_create(0, &cb);
		a = V21RX_create(0, &ca);
		if (a == 0 || b == 0) {
			diff_eq_int("both built (%ld)", a != 0 && b != 0, 1, k);
			continue;
		}

		da = V21RX_DSP(a);
		db = V21RX_DSP(b);
		ha = V21RX_HDX(a);
		hb = V21RX_HDX(b);
		maga = da->mag;
		magb = db->mag;
		tracea = da->fsd.trace;
		traceb = db->fsd.trace;

		/* Plant, keeping only the two sub-pointers. */
		memset(a, 0x5a, V21RX_OBJ_SIZE);
		memset(b, 0x5a, V21RX_OBJ_SIZE);
		V21RX_HDX(a) = ha;
		V21RX_DSP(a) = da;
		V21RX_HDX(b) = hb;
		V21RX_DSP(b) = db;

		b = ref_V21RX_create(b, &cb);
		a = V21RX_create(a, &ca);

		/* The second call must REUSE every allocation, not replace it. */
		diff_eq_int("dsp reused on re-init, ours (%ld)",
			    V21RX_DSP(a) == da, 1, k);
		diff_eq_int("dsp reused on re-init, blob (%ld)",
			    V21RX_DSP(b) == db, 1, k);
		diff_eq_int("hdx reused on re-init, ours (%ld)",
			    V21RX_HDX(a) == ha, 1, k);
		diff_eq_int("hdx reused on re-init, blob (%ld)",
			    V21RX_HDX(b) == hb, 1, k);
		diff_eq_int("mag reused on re-init, ours (%ld)",
			    V21RX_DSP(a)->mag == maga, 1, k);
		diff_eq_int("mag reused on re-init, blob (%ld)",
			    V21RX_DSP(b)->mag == magb, 1, k);
		diff_eq_int("fsd.trace reused on re-init, ours (%ld)",
			    V21RX_DSP(a)->fsd.trace == tracea, 1, k);
		diff_eq_int("fsd.trace reused on re-init, blob (%ld)",
			    V21RX_DSP(b)->fsd.trace == traceb, 1, k);

		/* Nothing of the planted pattern may survive. */
		diff_eq_int("status is not the planted pattern (%ld)",
			    V21RX_STATUS(a) != 0x5a, 1, k);
		diff_eq_int("flags are not the planted pattern (%ld)",
			    V21RX_FLAGS(a) != 0x5a, 1, k);

		compare_tree(cases[k].name, a, b, k);

		V21RX_delete(a);
		ref_V21RX_delete(b);
	}

	return diff_end();
}

/*
 * The facts about the result that hold without the blob, so that a bug
 * reproduced identically on both sides is still visible as a fact about V.21.
 */
static int
test_shape(void)
{
	struct v21rx_cfg c;
	void *a;
	long k;

	diff_begin("V21RX_create: what it builds, independently of the blob");

	for (k = 0; k < NCASES; k++) {
		struct v21_rx_dsp *d;

		build_cfg(&c, k);
		a = V21RX_create(0, &c);
		d = V21RX_DSP(a);

		/* The channel selector reaches BOTH tables it selects. */
		diff_eq_int("fsd.cfg.fir follows chan2 (%ld)",
			    d->fsd.cfg.fir == (cases[k].chan2
					       ? V21RX_CHAN2_INTRP
					       : V21RX_CHAN1_INTRP), 1, k);
		diff_eq_int("mtd.cfg.coeff follows chan2 (%ld)",
			    d->mtd->cfg.coeff == (cases[k].chan2
						  ? V21_CHAN2_MTD_COEFF
						  : V21_CHAN1_MTD_COEFF),
			    1, k);
		diff_eq_int("fsd.cfg.delay follows chan2 (%ld)",
			    d->fsd.cfg.delay, cases[k].chan2 ? 3 : 5, k);

		/*
		 * One bit is 24 samples at 7200 Hz and 300 bit/s, and the
		 * resampler is what produces 7200 from 8000.  Both halves of
		 * that arithmetic, so a change to either is caught.
		 */
		diff_eq_int("fsd.cfg.bit_samples is 7200/300 (%ld)",
			    d->fsd.cfg.bit_samples, 7200 / V21_STATUS_BPS, k);
		diff_eq_int("mrf resamples 8000 to 7200 (%ld)",
			    8000 * d->mrf.cfg.branches / d->mrf.cfg.decimate,
			    7200, k);
		diff_eq_int("mrf.history_len is taps/branches (%ld)",
			    d->mrf.history_len, 40, k);

		/* `mag` and the trace are the same length; see V21RX_MAG_BYTES. */
		diff_eq_int("mag holds trace_len shorts (%ld)",
			    V21RX_MAG_BYTES / 2, d->fsd.cfg.trace_len, k);

		/* The library default is inverted for V.21. */
		diff_eq_int("fsd.cfg.high_bit is inverted (%ld)",
			    d->fsd.cfg.high_bit, 0, k);
		diff_eq_int("FPM_FSD_CFG ships the other polarity (%ld)",
			    FPM_FSD_CFG.high_bit, 1, k);

		/* The seeded status word. */
		diff_eq_int("status is START (%ld)",
			    V21RX_STATUS(a), V21RX_STATUS_START, k);
		diff_eq_int("flags are bits 4 and 6 (%ld)",
			    V21RX_FLAGS(a),
			    V21RX_FLAG_BIT4 | V21RX_FLAG_BIT6, k);
		diff_eq_int("flags1 is clear (%ld)", V21RX_FLAGS1(a), 0, k);
		diff_eq_int("hdx.state is START (%ld)",
			    V21RX_HDX(a)->state, V21RX_STATE_START, k);

		/*
		 * D1181's claim: the four bytes over `f18`/`pad1a` are the
		 * config's `aux`, halves in the right order.
		 */
		diff_eq_int("fsd.cfg.f18 is aux's low half (%ld)",
			    (unsigned short)d->fsd.cfg.f18,
			    (unsigned short)cases[k].aux, k);
		diff_eq_int("fsd.cfg.pad1a is aux's high half (%ld)",
			    (unsigned short)d->fsd.cfg.pad1a,
			    (unsigned short)(cases[k].aux >> 16), k);
		diff_eq_int("mrf.cfg.aux is the config's aux (%ld)",
			    d->mrf.cfg.aux == (void *)cases[k].aux, 1, k);

		V21RX_delete(a);
	}

	/* The default table selects the answering side. */
	diff_eq_int("V21RX_CFG selects channel 2 (%ld)", V21RX_CFG.chan2, 1, 0);
	diff_eq_int("and the blob's does too (%ld)", ref_V21RX_CFG.chan2, 1, 0);

	return diff_end();
}

/* ------------------------------------------------------------------------- */
/* V21RX_control, reconfiguring a built instance in place                    */

static const struct {
	const char *name;
	int null_arg;
	int int_0004;
	unsigned char flags;
} ctl_cases[] = {
	{ "NULL arg",                  1,     0, 0 },
	{ "flags clear",               0, 60000, 0 },
	{ "SET_HDX_INT0000 only",      0, 60000, V21RXCTL_SET_HDX_INT0000 },
	{ "REINIT only",                0, 60000, V21RXCTL_REINIT },
	{ "SET_HDX_INT0000 + REINIT",   0, 60000,
	  (unsigned char)(V21RXCTL_SET_HDX_INT0000 | V21RXCTL_REINIT) },
};

#define NCTL_CASES	((long)(sizeof(ctl_cases) / sizeof(ctl_cases[0])))

static int
test_control(void)
{
	long k;

	diff_begin("V21RX_control: reconfigure a built instance in place");

	for (k = 0; k < NCTL_CASES; k++) {
		struct v21rx_cfg ca, cb;
		struct v21rx_ctl arga, argb;
		void *a, *b;
		int ra, rb;

		/* Always case 0 ("params NULL"): REINIT must re-derive the
		 * SAME configuration it was built with -- V21RX_control never
		 * touches `chan2`, so nothing here drives the table-selecting
		 * field through a transition the object was never built to
		 * survive (the hazard finding F9901 records for V17's
		 * `int_0014`/retrain). */
		build_cfg(&ca, 0);
		build_cfg(&cb, 0);

		b = ref_V21RX_create(0, &cb);
		a = V21RX_create(0, &ca);
		if (a == 0 || b == 0) {
			diff_eq_int("both built (%ld)", a != 0 && b != 0, 1,
				    k);
			continue;
		}

		memset(&arga, 0, sizeof arga);
		arga.int_0004 = ctl_cases[k].int_0004;
		arga.flags = ctl_cases[k].flags;
		argb = arga;

		rb = ref_V21RX_control(b, ctl_cases[k].null_arg ? NULL
							  : &argb);
		ra = V21RX_control(a, ctl_cases[k].null_arg ? NULL : &arga);

		diff_eq_int("V21RX_control return (%ld)", ra, rb, k);

		compare_tree(ctl_cases[k].name, a, b, k);

		V21RX_delete(a);
		ref_V21RX_delete(b);
	}

	return diff_end();
}

/* ------------------------------------------------------------------------- */

int
main(void)
{
	int rc = 0;

	rc |= test_self_allocating();
	rc |= test_reinit();
	rc |= test_control();
	rc |= test_shape();

	return rc;
}

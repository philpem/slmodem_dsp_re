/*
 * t_v21txcreate.c -- differential test of `V21TX_create` (.text 0x0992f0,
 *                    757 bytes) and the transmit half-duplex machine it
 *                    installs: `TxNextStateV21`, `TxHdxStartV21`,
 *                    `TxHdxIdleV21` and `TxHdxDataV21`.
 *
 * `FIFO_CFG`'s own default is `t_fifocreate.c`'s; this file's job is
 * everything downstream of it -- the chokepoint F9500 records.
 *
 * FOUR LAYERS.
 *
 *   1. WIRING.  `V21TX_create` self-allocating, over several configurations,
 *      comparing the handle's config bytes, the parameter block, the DSP
 *      block's non-deep fields, and the transmit FIFO it builds -- all
 *      against the blob.  `t_v21fax.c` already drives `ModDataV21` and
 *      `TxNoCarrierV21` (and, through them, `FPM_FSM_init`/`FPM_MRF_init`'s
 *      own internals) over 200,000-odd trials with a SYNTHETIC handle, so
 *      this layer does not re-walk `fpm_tone`'s internals: it checks that
 *      `V21TX_create` reaches the same DSP configuration that layer, not
 *      that the DSP block behaves correctly once reached.
 *
 *   2. RE-INITIALISATION IN PLACE, D955/F8587 planted.
 *
 *   3. THE HALF-DUPLEX MACHINE THROUGH `V21TX_modem`, end to end, over many
 *      consecutive blocks with the FIFO sometimes fed and sometimes left to
 *      run dry -- which is what drives the machine around its
 *      START -> DATA -> IDLE -> START cycle and through `TxHdxDataV21`'s
 *      three arms.
 *
 *   4. THE TWO ARMS LAYER 3 CANNOT REACH RELIABLY: `TxHdxDataV21`'s
 *      underrun-with-`V21TXP_INT_0004`-set arm (nothing reconstructed ever
 *      sets that field non-zero, so `V21TX_modem` alone never takes it) and
 *      `TxNextStateV21`'s out-of-range default arm.  Both are reached by
 *      calling the half-duplex functions directly against a handle
 *      `V21TX_create` built, with the one field a real caller has no route
 *      to poked by offset -- the same idiom `t_v21fax.c` already uses for
 *      its own synthetic transmit object.
 */

#include <stddef.h>
#include <stdio.h>
#include <string.h>

#include "harness.h"

#include "dsplib/faxfifo.h"
#include "dsplib/fpm_fsm.h"
#include "dsplib/fpm_mrf.h"
#include "dsplib/sysdep.h"
#include "dsplib/v21cfg.h"
#include "dsplib/v21fax.h"

extern void *ref_V21TX_create(void *modem, const struct v21tx_cfg *params);
extern void ref_V21TX_delete(void *modem);
extern int ref_V21TX_control(void *modem, const struct v21tx_ctl *arg);
extern int ref_V21TX_modem(void *modem, unsigned short *in, short *out,
			   unsigned short *count);
extern short ref_TxHdxStartV21(void *modem, unsigned short *in, short *out,
			       short *budget);
extern short ref_TxHdxIdleV21(void *modem, unsigned short *in, short *out,
			      short *budget);
extern short ref_TxHdxDataV21(void *modem, unsigned short *in, short *out,
			      short *budget);
extern void ref_TxNextStateV21(void *modem);
extern struct v21tx_cfg ref_V21TX_CFG;
extern const struct fifo_cfg ref_FIFO_CFG;
extern struct fpm_fsm_cfg ref_FPM_FSM_CFG;
extern int ref_FIFO_write(struct fax_fifo *f, unsigned short *src,
			  unsigned short count);

#define FIELD(obj, off)		((unsigned char *)(void *)(obj) + (off))
#define FIELD_PTR(obj, off) \
	(*(void **)(void *)FIELD((obj), (off)))
#define AT_S(obj, off)		(*(short *)(void *)FIELD((obj), (off)))
#define AT_US(obj, off)		(*(unsigned short *)(void *)FIELD((obj), (off)))

/* ------------------------------------------------------------------------- */

static void
cmp_shorts(const char *what, const short *got, const short *want, int n)
{
	int i;

	for (i = 0; i < n; i++) {
		if (got[i] != want[i]) {
			diff_eq_int(what, got[i], want[i], i);
			return;
		}
	}
	diff_eq_int(what, 0, 0, n);
}

static void
cmp_ushorts(const char *what, const unsigned short *got,
	    const unsigned short *want, int n)
{
	int i;

	for (i = 0; i < n; i++) {
		if (got[i] != want[i]) {
			diff_eq_int(what, got[i], want[i], i);
			return;
		}
	}
	diff_eq_int(what, 0, 0, n);
}

/* Which of the three transmit handlers a pointer is, on EITHER side. */
static int
fn_id(const void *p)
{
	if (p == (const void *)TxHdxStartV21
	    || p == (const void *)&ref_TxHdxStartV21)
		return 0;
	if (p == (const void *)TxHdxDataV21
	    || p == (const void *)&ref_TxHdxDataV21)
		return 1;
	if (p == (const void *)TxHdxIdleV21
	    || p == (const void *)&ref_TxHdxIdleV21)
		return 2;
	return -1;
}

/* ------------------------------------------------------------------------- */

static int
test_shape(void)
{
	diff_begin("v21txcreate: sizes against the object's symbol table");

	diff_eq_int("sizeof(struct v21tx_cfg) (%ld)",
		    (long)sizeof(struct v21tx_cfg), 0x1c, 0);
	diff_eq_int("sizeof V21TX_CFG (%ld)", (long)sizeof(V21TX_CFG),
		    0x1c, 0);
	diff_eq_int("V21TX_OBJ_SIZE (%ld)", V21TX_OBJ_SIZE, 0x28, 0);
	diff_eq_int("sizeof(struct v21_tx_dsp) (%ld)",
		    (long)sizeof(struct v21_tx_dsp), 0x30, 0);

	return diff_end();
}

/*
 * D1230.  Two objects the blob keeps as one: `FPM_FSM_CFG` (the object's own
 * name, referenced directly by `V21TX_create`) and `FPM_FSM_CFG_data` (this
 * tree's older stub, `src/pump/b103/b103fp.c`'s reader).  Must be
 * `memcmp`-identical or the duplicate is a divergence rather than a
 * redundancy -- `t_v21cfg.c` runs the same check for `FPM_FSD_CFG`/D1180.
 */
static int
test_fsm_cfg_duplicate(void)
{
	diff_begin("v21txcreate: FPM_FSM_CFG equals FPM_FSM_CFG_data (D1230)");

	diff_eq_int("FPM_FSM_CFG_data equals FPM_FSM_CFG (%ld)",
		    memcmp(&FPM_FSM_CFG_data, &FPM_FSM_CFG,
			   sizeof(FPM_FSM_CFG)) == 0, 1, 0);
	diff_eq_int("ours equals the blob's own FPM_FSM_CFG (%ld)",
		    memcmp(&FPM_FSM_CFG, &ref_FPM_FSM_CFG,
			   sizeof(FPM_FSM_CFG)) == 0, 1, 0);

	return diff_end();
}

/*
 * Everything `V21TX_create` builds, laid out to the DSP block's shallow
 * fields -- see the file header for why `fpm_tone`'s own internals are not
 * re-walked here.
 */
static void
compare_tree_ex(const char *what, void *a, void *b, long tag,
		 int expect_default_scale)
{
	void *pa = FIELD_PTR(a, V21TX_OBJ_PARAMS);
	void *pb = FIELD_PTR(b, V21TX_OBJ_PARAMS);
	struct v21_tx_dsp *da = (struct v21_tx_dsp *)
		FIELD_PTR(a, V21TX_OBJ_DSP);
	struct v21_tx_dsp *db = (struct v21_tx_dsp *)
		FIELD_PTR(b, V21TX_OBJ_DSP);
	struct fax_fifo *fa, *fb;
	char buf[160];

	diff_eq_int("params allocated on both sides (%ld)",
		    pa != 0 && pb != 0, 1, tag);
	diff_eq_int("dsp allocated on both sides (%ld)",
		    da != 0 && db != 0, 1, tag);
	if (pa == 0 || pb == 0 || da == 0 || db == 0)
		return;

	/* The config, the handle's first 28 bytes -- write-only, but still
	 * a claim: whatever V21TX_create copied in, both sides copied the
	 * same bytes. */
	snprintf(buf, sizeof(buf), "%.90s config bytes (%%ld)", what);
	diff_eq_int(buf, memcmp(a, b, sizeof(struct v21tx_cfg)) == 0, 1, tag);

	/* The result word: status, and the two flag bytes F9500 named. */
	snprintf(buf, sizeof(buf), "%.90s result status (%%ld)", what);
	diff_eq_int(buf, *FIELD(a, V21TX_OBJ_RESULT),
		    *FIELD(b, V21TX_OBJ_RESULT), tag);
	snprintf(buf, sizeof(buf), "%.90s result B1 (%%ld)", what);
	diff_eq_int(buf, *FIELD(a, V21TX_OBJ_RESULT_B1),
		    *FIELD(b, V21TX_OBJ_RESULT_B1), tag);
	snprintf(buf, sizeof(buf), "%.90s result B2 (%%ld)", what);
	diff_eq_int(buf, *FIELD(a, V21TX_OBJ_RESULT_B2),
		    *FIELD(b, V21TX_OBJ_RESULT_B2), tag);

	/* The parameter block: FIFO built, dispatch slot at START, state
	 * fields zeroed. */
	snprintf(buf, sizeof(buf), "%.90s params.int_0004 (%%ld)", what);
	diff_eq_int(buf, *(int *)(void *)FIELD(pa, V21TXP_INT_0004),
		    *(int *)(void *)FIELD(pb, V21TXP_INT_0004), tag);
	snprintf(buf, sizeof(buf), "%.90s params.state (%%ld)", what);
	diff_eq_int(buf, AT_S(pa, V21TXP_STATE), AT_S(pb, V21TXP_STATE), tag);
	snprintf(buf, sizeof(buf), "%.90s params.short_000e (%%ld)", what);
	diff_eq_int(buf, AT_S(pa, V21TXP_SHORT_000E),
		    AT_S(pb, V21TXP_SHORT_000E), tag);
	snprintf(buf, sizeof(buf), "%.90s params.process, ours vs blob's (%%ld)",
		 what);
	diff_eq_int(buf, fn_id(FIELD_PTR(pa, V21TXP_PROCESS)),
		    fn_id(FIELD_PTR(pb, V21TXP_PROCESS)), tag);

	fa = (struct fax_fifo *)FIELD_PTR(pa, V21TXP_FIFO);
	fb = (struct fax_fifo *)FIELD_PTR(pb, V21TXP_FIFO);
	diff_eq_int("FIFO built on both sides (%ld)", fa != 0 && fb != 0,
		    1, tag);
	if (fa != 0 && fb != 0) {
		/* The literals F9500 derived: word0 from FIFO_CFG (0 either
		 * copy), size 6, fill 1 -- NOT FIFO_CFG's own size. */
		diff_eq_int("fifo.short_000 (%ld)", fa->short_000,
			    fb->short_000, tag);
		diff_eq_int("fifo.size is 6, not FIFO_CFG.size (%ld)",
			    fa->size, 6, tag);
		diff_eq_int("fifo.fill is 1 (%ld)", fa->fill, 1, tag);
		diff_eq_int("fifo.count (%ld)", fa->count, fb->count, tag);
		diff_eq_int("fifo.rd (%ld)", fa->rd, fb->rd, tag);
		diff_eq_int("fifo.wr (%ld)", fa->wr, fb->wr, tag);
		cmp_ushorts("fifo.buf[%ld]", fa->buf, fb->buf, 6);
	}

	/* The DSP block's shallow fields -- the modulator's config (with
	 * F9500/D1241's scale override) and the resampler's, not the tone
	 * generator's or the resampler history's internals. */
	diff_eq_int("dsp.fsm.cfg.freq[0] (%ld)", da->fsm.cfg.freq[0],
		    db->fsm.cfg.freq[0], tag);
	diff_eq_int("dsp.fsm.cfg.freq[1] (%ld)", da->fsm.cfg.freq[1],
		    db->fsm.cfg.freq[1], tag);
	diff_eq_int("dsp.fsm.cfg.samples_per_sym (%ld)",
		    da->fsm.cfg.samples_per_sym, db->fsm.cfg.samples_per_sym,
		    tag);
	diff_eq_int("dsp.fsm.cfg.scale (%ld)", da->fsm.cfg.scale,
		    db->fsm.cfg.scale, tag);
	if (expect_default_scale)
		diff_eq_int("dsp.fsm.cfg.scale is 0x1900 (%ld)",
			    da->fsm.cfg.scale, 0x1900, tag);
	diff_eq_int("dsp.fsm.tone built on both sides (%ld)",
		    da->fsm.tone != 0 && db->fsm.tone != 0, 1, tag);

	diff_eq_int("dsp.mrf.cfg.branches (%ld)", da->mrf.cfg.branches,
		    db->mrf.cfg.branches, tag);
	diff_eq_int("dsp.mrf.cfg.branches is 10 (%ld)", da->mrf.cfg.branches,
		    10, tag);
	diff_eq_int("dsp.mrf.cfg.decimate (%ld)", da->mrf.cfg.decimate,
		    db->mrf.cfg.decimate, tag);
	diff_eq_int("dsp.mrf.cfg.decimate is 9 (%ld)", da->mrf.cfg.decimate,
		    9, tag);
	diff_eq_int("dsp.mrf.cfg.taps (%ld)", da->mrf.cfg.taps,
		    db->mrf.cfg.taps, tag);
	cmp_shorts("dsp.mrf.cfg.coeff[%ld]", da->mrf.cfg.coeff,
		   db->mrf.cfg.coeff, 360);
	/* D1242: dsp.mrf.cfg.aux is EXCLUDED, deliberately. */
	diff_eq_int("dsp.mrf.history_len (%ld)", da->mrf.history_len,
		    db->mrf.history_len, tag);
	cmp_shorts("dsp.mrf.history[%ld]", da->mrf.history, db->mrf.history,
		   da->mrf.history_len < db->mrf.history_len
			   ? da->mrf.history_len : db->mrf.history_len);

	diff_eq_int("dsp.scratch built on both sides (%ld)",
		    da->scratch != 0 && db->scratch != 0, 1, tag);
	if (da->scratch != 0 && db->scratch != 0)
		cmp_shorts("dsp.scratch[%ld]", da->scratch, db->scratch,
			   V21TX_SCRATCH_BYTES / 2);
}

/*
 * The scale invariant holds for every caller except `test_control`, which
 * deliberately overrides `dsp.fsm.cfg.scale` through `V21TX_control`.
 */
static void
compare_tree(const char *what, void *a, void *b, long tag)
{
	compare_tree_ex(what, a, b, tag, 1);
}

/* ------------------------------------------------------------------------- */

static const struct {
	const char *name;
	int use_default;
	short short_0000;
} cases[] = {
	{ "params NULL (V21TX_CFG)",         1, 0 },
	{ "explicit, short_0000 == 0",       0, 0 },
	{ "explicit, short_0000 == 1",       0, 1 },
	{ "explicit, short_0000 == 2 (default arm)", 0, 2 },
	{ "explicit, short_0000 == -1 (default arm)", 0, -1 }
};
#define NCASES ((long)(sizeof(cases) / sizeof(cases[0])))

static void
build_cfg(struct v21tx_cfg *c, long k)
{
	*c = V21TX_CFG;
	c->short_0000 = cases[k].short_0000;
}

static int
test_create_self_allocating(void)
{
	long k;

	diff_begin("v21txcreate: V21TX_create, self-allocating");

	for (k = 0; k < NCASES; k++) {
		struct v21tx_cfg ca, cb;
		void *a, *b;

		build_cfg(&ca, k);
		build_cfg(&cb, k);

		if (cases[k].use_default) {
			b = ref_V21TX_create(0, 0);
			a = V21TX_create(0, 0);
		} else {
			b = ref_V21TX_create(0, &cb);
			a = V21TX_create(0, &ca);
		}

		diff_eq_int("both built (%ld)", a != 0 && b != 0, 1, k);
		if (a == 0 || b == 0)
			continue;

		compare_tree(cases[k].name, a, b, k);

		/*
		 * Fresh from `V21TX_create`, before anything has run:
		 * `TxHdxStartV21` at `V21TX_STATE_START`.  Only true right
		 * after construction -- `compare_tree` itself does not
		 * assert it, since it is also used after the machine has run.
		 */
		{
			void *pa = FIELD_PTR(a, V21TX_OBJ_PARAMS);

			diff_eq_int("fresh: params.state is V21TX_STATE_START (%ld)",
				    AT_S(pa, V21TXP_STATE), V21TX_STATE_START,
				    k);
			diff_eq_int("fresh: params.process is TxHdxStartV21 (%ld)",
				    fn_id(FIELD_PTR(pa, V21TXP_PROCESS)), 0, k);
		}

		/*
		 * `short_0000`'s one observable effect: the "neither 0 nor
		 * 1" branch alone raises V21TX_RESULT_B1_BIT1 and reports
		 * V21TX_STATUS_DEFAULT.  Checked directly, not only through
		 * the blob comparison above.
		 */
		if (!cases[k].use_default && cases[k].short_0000 != 0
		    && cases[k].short_0000 != 1) {
			diff_eq_int("default-arm status is V21TX_STATUS_DEFAULT (%ld)",
				    *FIELD(a, V21TX_OBJ_RESULT),
				    V21TX_STATUS_DEFAULT, k);
			diff_eq_int("default-arm sets RESULT_B1_BIT1 (%ld)",
				    (*FIELD(a, V21TX_OBJ_RESULT_B1)
				     & V21TX_RESULT_B1_BIT1) != 0, 1, k);
		} else if (!cases[k].use_default) {
			/*
			 * short_0000 in {0, 1}: no further write to the
			 * status byte after V21TX_create's own zeroing of
			 * the whole result word (0x09934d/0x099360) -- it
			 * only becomes V21TX_STATUS_START once TxHdxStartV21
			 * actually RUNS, which construction alone does not
			 * do.
			 */
			diff_eq_int("0/1 arm status stays 0 at construction (%ld)",
				    *FIELD(a, V21TX_OBJ_RESULT), 0, k);
		}

		V21TX_delete(a);
		ref_V21TX_delete(b);
	}

	return diff_end();
}

/*
 * Re-initialisation in place, D955/F8587 planted: the whole handle and its
 * FIFO's buffer are overwritten with a non-zero pattern between two calls
 * (keeping only the two sub-pointers, which the constructor tests to decide
 * whether to allocate), so a field the second call forgets to rewrite is
 * visibly the pattern rather than invisibly a zero the first call already
 * left.
 */
static int
test_reinit(void)
{
	void *a, *b;
	void *pa, *pb;
	void *dspa, *dspb;
	struct v21tx_cfg ca, cb;

	diff_begin("v21txcreate: V21TX_create, re-initialised in place");

	ca = V21TX_CFG;
	cb = V21TX_CFG;
	a = V21TX_create(0, &ca);
	b = ref_V21TX_create(0, &cb);
	diff_eq_int("both built (%ld)", a != 0 && b != 0, 1, 0);
	if (a == 0 || b == 0)
		return diff_end();

	pa = FIELD_PTR(a, V21TX_OBJ_PARAMS);
	pb = FIELD_PTR(b, V21TX_OBJ_PARAMS);
	dspa = FIELD_PTR(a, V21TX_OBJ_DSP);
	dspb = FIELD_PTR(b, V21TX_OBJ_DSP);

	memset(a, 0x5a, V21TX_OBJ_SIZE);
	memset(b, 0x5a, V21TX_OBJ_SIZE);
	FIELD_PTR(a, V21TX_OBJ_PARAMS) = pa;
	FIELD_PTR(b, V21TX_OBJ_PARAMS) = pb;
	FIELD_PTR(a, V21TX_OBJ_DSP) = dspa;
	FIELD_PTR(b, V21TX_OBJ_DSP) = dspb;
	/* And the FIFO pointer inside the params block, for the same
	 * reason -- FIFO_create's own re-init contract, F9500. */

	a = V21TX_create(a, &ca);
	b = ref_V21TX_create(b, &cb);

	diff_eq_int("params kept, ours (%ld)",
		    FIELD_PTR(a, V21TX_OBJ_PARAMS) == pa, 1, 0);
	diff_eq_int("params kept, blob's (%ld)",
		    FIELD_PTR(b, V21TX_OBJ_PARAMS) == pb, 1, 0);
	diff_eq_int("dsp kept, ours (%ld)",
		    FIELD_PTR(a, V21TX_OBJ_DSP) == dspa, 1, 0);
	diff_eq_int("dsp kept, blob's (%ld)",
		    FIELD_PTR(b, V21TX_OBJ_DSP) == dspb, 1, 0);

	compare_tree("reinit", a, b, 0);

	/*
	 * ALWAYS installs TxHdxStartV21 at V21TX_STATE_START, whether or not
	 * `params`/`dsp` already existed -- 0x0993a8/0x0993af are
	 * unconditional, past the branch that skips (re)allocating.
	 */
	diff_eq_int("reinit: params.state is V21TX_STATE_START (%ld)",
		    AT_S(pa, V21TXP_STATE), V21TX_STATE_START, 0);
	diff_eq_int("reinit: params.process is TxHdxStartV21 (%ld)",
		    fn_id(FIELD_PTR(pa, V21TXP_PROCESS)), 0, 0);

	V21TX_delete(a);
	ref_V21TX_delete(b);

	return diff_end();
}

/*
 * The half-duplex machine through `V21TX_modem`, over many consecutive
 * blocks. `queue` feeds the transmit FIFO by hand -- there is no
 * reconstructed producer for it, `faxvmi.c` is outside this pass -- so a
 * block sometimes has data queued and sometimes does not, which is what
 * carries the machine through V21TX_STATE_START -> DATA -> IDLE -> START.
 */
static unsigned
rnd(unsigned *s)
{
	*s = (*s) * 1103515245u + 12345u;
	return (*s >> 16) & 0x7fffu;
}

static int
test_modem_cycle(void)
{
	void *a, *b;
	struct v21tx_cfg ca, cb;
	unsigned short outa[4096], outb[4096];
	unsigned seed = 424242u;
	int block;

	diff_begin("v21txcreate: the half-duplex machine through V21TX_modem");

	ca = V21TX_CFG;
	cb = V21TX_CFG;
	a = V21TX_create(0, &ca);
	b = ref_V21TX_create(0, &cb);
	diff_eq_int("both built (%ld)", a != 0 && b != 0, 1, 0);
	if (a == 0 || b == 0)
		return diff_end();

	for (block = 0; block < 40; block++) {
		/*
		 * `V21TX_modem`'s own convention: `in`/`*count` on entry is
		 * the block of INPUT elements available to enqueue (via its
		 * own internal `FIFO_write`, since `V21TXP_INT_0004` stays 0
		 * -- V21TX_create's own default), not an output buffer size.
		 * Every element is initialised, so a block with `qn == 0`
		 * queues nothing and the machine runs on whatever the FIFO
		 * already had -- which is what drives it into IDLE.
		 */
		unsigned short qbuf_a[8], qbuf_b[8];
		int qn = (int)(rnd(&seed) % 9);
		int i;
		unsigned short counta, countb;
		int reta, retb;

		for (i = 0; i < qn; i++)
			qbuf_a[i] = qbuf_b[i] =
				(unsigned short)(rnd(&seed) & 1);

		counta = countb = (unsigned short)qn;
		memset(outa, 0xa5, sizeof outa);
		memset(outb, 0xa5, sizeof outb);

		reta = V21TX_modem(a, qbuf_a, (short *)outa, &counta);
		retb = ref_V21TX_modem(b, qbuf_b, (short *)outb, &countb);

		diff_eq_int("V21TX_modem return (%ld)", reta, retb, block);
		diff_eq_int("V21TX_modem *count (%ld)", counta, countb, block);
		if (counta == countb && counta <= 4096)
			cmp_ushorts("out[%ld]", outa, outb, counta);
	}

	compare_tree("after modem cycle", a, b, 0);

	V21TX_delete(a);
	ref_V21TX_delete(b);

	return diff_end();
}

/*
 * The two arms `V21TX_modem` alone cannot reliably reach: `TxHdxDataV21`'s
 * `V21TXP_INT_0004 != 0` underrun arm, and `TxNextStateV21`'s out-of-range
 * default arm.  Both are reached by calling the half-duplex functions
 * directly, `V21TX_create`-built handles with one field poked by offset --
 * `t_v21fax.c`'s own idiom for its synthetic transmit object.
 */
static int
test_underrun_bypass_arm(void)
{
	void *a, *b;
	struct v21tx_cfg ca, cb;
	void *pa, *pb;
	struct fax_fifo *fa, *fb;
	unsigned short ina[8], inb[8], qa[3], qb[3];
	/*
	 * `ModDataV21` can produce up to `samples_per_sym` (24) output
	 * samples per bit -- 8 requested bits worth is comfortably under
	 * 256; a too-small buffer here is a stack overflow, not a failing
	 * comparison.
	 */
	short outa[256], outb[256];
	short budgeta, budgetb;
	short reta, retb;

	diff_begin("v21txcreate: TxHdxDataV21's V21TXP_INT_0004 != 0 underrun arm");

	ca = V21TX_CFG;
	cb = V21TX_CFG;
	a = V21TX_create(0, &ca);
	b = ref_V21TX_create(0, &cb);
	diff_eq_int("both built (%ld)", a != 0 && b != 0, 1, 0);
	if (a == 0 || b == 0)
		return diff_end();

	pa = FIELD_PTR(a, V21TX_OBJ_PARAMS);
	pb = FIELD_PTR(b, V21TX_OBJ_PARAMS);
	fa = (struct fax_fifo *)FIELD_PTR(pa, V21TXP_FIFO);
	fb = (struct fax_fifo *)FIELD_PTR(pb, V21TXP_FIFO);

	/* Queue 3 elements, ask TxHdxDataV21 for 8: an underrun. */
	qa[0] = qa[1] = qa[2] = 1;
	qb[0] = qb[1] = qb[2] = 1;
	FIFO_write(fa, qa, 3);
	ref_FIFO_write(fb, qb, 3);

	*(int *)(void *)FIELD(pa, V21TXP_INT_0004) = 1;
	*(int *)(void *)FIELD(pb, V21TXP_INT_0004) = 1;
	AT_S(pa, V21TXP_STATE) = V21TX_STATE_DATA;
	AT_S(pb, V21TXP_STATE) = V21TX_STATE_DATA;

	memset(ina, 0xa5, sizeof ina);
	memset(inb, 0xa5, sizeof inb);
	memset(outa, 0xa5, sizeof outa);
	memset(outb, 0xa5, sizeof outb);
	budgeta = budgetb = 8;

	reta = TxHdxDataV21(a, ina, outa, &budgeta);
	retb = ref_TxHdxDataV21(b, inb, outb, &budgetb);

	diff_eq_int("return (%ld)", reta, retb, 0);
	diff_eq_int("*budget LEFT NON-ZERO (%ld)", budgeta, budgetb, 0);
	diff_eq_int("*budget is 5 (8 requested - 3 taken) (%ld)", budgeta,
		    5, 0);
	cmp_shorts("out[%ld]", outa, outb, reta < retb ? reta : retb);
	diff_eq_int("params.state advanced, ours (%ld)",
		    fn_id(FIELD_PTR(pa, V21TXP_PROCESS)), 2, 0);
	diff_eq_int("params.state advanced, blob's (%ld)",
		    fn_id(FIELD_PTR(pb, V21TXP_PROCESS)), 2, 0);
	diff_eq_int("params.state is V21TX_STATE_IDLE, ours (%ld)",
		    AT_S(pa, V21TXP_STATE), V21TX_STATE_IDLE, 0);

	V21TX_delete(a);
	ref_V21TX_delete(b);

	return diff_end();
}

static int
test_next_state_default_arm(void)
{
	void *a, *b;
	struct v21tx_cfg ca, cb;
	void *pa, *pb;
	short states[3] = { 3, -5, 100 };
	int i;

	diff_begin("v21txcreate: TxNextStateV21's out-of-range default arm");

	ca = V21TX_CFG;
	cb = V21TX_CFG;
	a = V21TX_create(0, &ca);
	b = ref_V21TX_create(0, &cb);
	diff_eq_int("both built (%ld)", a != 0 && b != 0, 1, 0);
	if (a == 0 || b == 0)
		return diff_end();

	pa = FIELD_PTR(a, V21TX_OBJ_PARAMS);
	pb = FIELD_PTR(b, V21TX_OBJ_PARAMS);

	for (i = 0; i < 3; i++) {
		AT_S(pa, V21TXP_STATE) = states[i];
		AT_S(pb, V21TXP_STATE) = states[i];
		*FIELD(a, V21TX_OBJ_RESULT_B1) = 0;
		*FIELD(b, V21TX_OBJ_RESULT_B1) = 0;
		*FIELD(a, V21TX_OBJ_RESULT_B2) = 0xff;
		*FIELD(b, V21TX_OBJ_RESULT_B2) = 0xff;

		TxNextStateV21(a);
		ref_TxNextStateV21(b);

		diff_eq_int("state unchanged, ours (%ld)",
			    AT_S(pa, V21TXP_STATE), states[i], i);
		diff_eq_int("state unchanged, blob's (%ld)",
			    AT_S(pb, V21TXP_STATE), states[i], i);
		diff_eq_int("status is V21TX_STATUS_DEFAULT (%ld)",
			    *FIELD(a, V21TX_OBJ_RESULT), V21TX_STATUS_DEFAULT,
			    i);
		diff_eq_int("RESULT_B1 (%ld)", *FIELD(a, V21TX_OBJ_RESULT_B1),
			    *FIELD(b, V21TX_OBJ_RESULT_B1), i);
		diff_eq_int("RESULT_B1_BIT1 set (%ld)",
			    (*FIELD(a, V21TX_OBJ_RESULT_B1)
			     & V21TX_RESULT_B1_BIT1) != 0, 1, i);
		diff_eq_int("RESULT_B2 (%ld)", *FIELD(a, V21TX_OBJ_RESULT_B2),
			    *FIELD(b, V21TX_OBJ_RESULT_B2), i);
		diff_eq_int("RESULT_B2_BIT0 cleared (%ld)",
			    (*FIELD(a, V21TX_OBJ_RESULT_B2)
			     & V21TX_RESULT_B2_BIT0) != 0, 0, i);
	}

	V21TX_delete(a);
	ref_V21TX_delete(b);

	return diff_end();
}

/* ------------------------------------------------------------------------- */
/* V21TX_control, reconfiguring a built instance in place                    */

static const struct {
	const char *name;
	int null_arg;
	int int_0004;
	int int_0008;
	unsigned char flags_0c;
	unsigned char flags_0d;
} tctl_cases[] = {
	{ "NULL arg",                     1,     0,     0, 0, 0 },
	{ "flags clear",                  0, 60000,   500, 0, 0 },
	{ "SET_TXFLAGS_BIT2 only",        0, 60000,  1000,
	  V21TXCTL_SET_TXFLAGS_BIT2, 0 },
	{ "SET_PARAMS_INT0004 only",      0, 60000, -1000, 0,
	  V21TXCTL_SET_PARAMS_INT0004 },
	{ "REINIT only",                  0, 60000,     0, 0, V21TXCTL_REINIT },
	{ "everything at once",           0, 60000,  2000,
	  V21TXCTL_SET_TXFLAGS_BIT2,
	  (unsigned char)(V21TXCTL_SET_PARAMS_INT0004 | V21TXCTL_REINIT) },
};

#define NTCTL_CASES ((long)(sizeof(tctl_cases) / sizeof(tctl_cases[0])))

static int
test_control(void)
{
	long k;

	diff_begin("v21txcreate: V21TX_control, reconfigure in place");

	for (k = 0; k < NTCTL_CASES; k++) {
		struct v21tx_cfg ca, cb;
		struct v21tx_ctl arga, argb;
		void *a, *b;
		int ra, rb;

		/* Always case 0 ("params NULL"): REINIT re-derives the SAME
		 * configuration it was built with. */
		build_cfg(&ca, 0);
		build_cfg(&cb, 0);

		b = ref_V21TX_create(0, &cb);
		a = V21TX_create(0, &ca);
		if (a == 0 || b == 0) {
			diff_eq_int("both built (%ld)", a != 0 && b != 0, 1,
				    k);
			continue;
		}

		memset(&arga, 0, sizeof arga);
		arga.int_0004 = tctl_cases[k].int_0004;
		arga.int_0008 = tctl_cases[k].int_0008;
		arga.flags_0c = tctl_cases[k].flags_0c;
		arga.flags_0d = tctl_cases[k].flags_0d;
		argb = arga;

		rb = ref_V21TX_control(b, tctl_cases[k].null_arg ? NULL
							   : &argb);
		ra = V21TX_control(a, tctl_cases[k].null_arg ? NULL : &arga);

		diff_eq_int("V21TX_control return (%ld)", ra, rb, k);

		compare_tree_ex(tctl_cases[k].name, a, b, k, 0);

		V21TX_delete(a);
		ref_V21TX_delete(b);
	}

	return diff_end();
}

/* ------------------------------------------------------------------------- */

int
main(void)
{
	int rc = 0;

	rc |= test_shape();
	rc |= test_fsm_cfg_duplicate();
	rc |= test_create_self_allocating();
	rc |= test_reinit();
	rc |= test_modem_cycle();
	rc |= test_underrun_bypass_arm();
	rc |= test_next_state_default_arm();
	rc |= test_control();

	return rc;
}

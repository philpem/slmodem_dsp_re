/*
 * t_v29txcreate.c -- differential test of `V29TX_create` (.text 0x09ba00,
 *                    1,162 bytes) and the transmit half-duplex machine it
 *                    installs: `TxNextStateV29` and the seven `TxHdx*V29`
 *                    states.
 *
 * `SGD_CTL` (F9700) was the sole external blocker; this file is everything
 * downstream of it, finding F9701.
 *
 * FOUR LAYERS, `t_v21txcreate.c`'s own shape one modulation over.
 *
 *   1. WIRING.  `V29TX_create` self-allocating over several bit rates,
 *      comparing the handle's config bytes, the parameter block and the
 *      private TX block's shallow fields against the blob.
 *
 *   2. RE-INITIALISATION IN PLACE, D955/F8587 planted.
 *
 *   3. THE HALF-DUPLEX MACHINE THROUGH `V29TX_modem`, over many consecutive
 *      blocks with the FIFO sometimes fed and sometimes left to run dry --
 *      which is what drives the machine around its full seven-state cycle,
 *      START -> QUIET -> ALT -> EQCOND -> SCR1 -> DATA -> IDLE -> START.
 *
 *   4. THE ARM LAYER 3 CANNOT REACH RELIABLY: `TxHdxDataV29`'s
 *      underrun-with-`V29TXP_INT_0008`-set arm (nothing reconstructed ever
 *      sets that field non-zero) and `TxNextStateV29`'s out-of-range
 *      default arm.  Both reached by calling the half-duplex functions
 *      directly against a handle `V29TX_create` built, `t_v21txcreate.c`'s
 *      own idiom.
 */

#include <stddef.h>
#include <stdio.h>
#include <string.h>

#include "harness.h"

#include "dsplib/faxfifo.h"
#include "dsplib/fpm_pps.h"
#include "dsplib/sdm.h"
#include "dsplib/sgd.h"
#include "dsplib/smc.h"
#include "dsplib/sysdep.h"
#include "dsplib/v29data.h"
#include "dsplib/v29fax.h"

extern void *ref_V29TX_create(void *modem, const struct v29tx_cfg *params);
extern void ref_V29TX_delete(void *modem);
extern int ref_V29TX_modem(void *modem, unsigned short *in, short *out,
			   unsigned short *count);
extern short ref_TxHdxStartV29(void *modem, unsigned short *in, short *out,
			       short *budget);
extern short ref_TxHdxQuietV29(void *modem, unsigned short *in, short *out,
			       short *budget);
extern short ref_TxHdxABV29(void *modem, unsigned short *in, short *out,
			    short *budget);
extern short ref_TxHdxEQCondV29(void *modem, unsigned short *in, short *out,
				short *budget);
extern short ref_TxHdxSCR1V29(void *modem, unsigned short *in, short *out,
			      short *budget);
extern short ref_TxHdxDataV29(void *modem, unsigned short *in, short *out,
			      short *budget);
extern short ref_TxHdxIdleV29(void *modem, unsigned short *in, short *out,
			      short *budget);
extern void ref_TxNextStateV29(void *modem);
extern struct v29tx_cfg ref_V29TX_CFG;

#define FIELD(obj, off)		((unsigned char *)(void *)(obj) + (off))
#define FIELD_PTR(obj, off)	(*(void **)(void *)FIELD((obj), (off)))
#define AT_S(obj, off)		(*(short *)(void *)FIELD((obj), (off)))
#define AT_I(obj, off)		(*(int *)(void *)FIELD((obj), (off)))

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

/* Which of the seven transmit handlers a pointer is, on EITHER side. */
static int
fn_id(const void *p)
{
	if (p == (const void *)TxHdxStartV29
	    || p == (const void *)&ref_TxHdxStartV29)
		return 0;
	if (p == (const void *)TxHdxQuietV29
	    || p == (const void *)&ref_TxHdxQuietV29)
		return 1;
	if (p == (const void *)TxHdxABV29
	    || p == (const void *)&ref_TxHdxABV29)
		return 2;
	if (p == (const void *)TxHdxEQCondV29
	    || p == (const void *)&ref_TxHdxEQCondV29)
		return 3;
	if (p == (const void *)TxHdxSCR1V29
	    || p == (const void *)&ref_TxHdxSCR1V29)
		return 4;
	if (p == (const void *)TxHdxDataV29
	    || p == (const void *)&ref_TxHdxDataV29)
		return 5;
	if (p == (const void *)TxHdxIdleV29
	    || p == (const void *)&ref_TxHdxIdleV29)
		return 6;
	return -1;
}

/* ------------------------------------------------------------------------- */

static int
test_shape(void)
{
	diff_begin("v29txcreate: sizes against the object's symbol table");

	diff_eq_int("sizeof(struct v29tx_cfg) (%ld)",
		    (long)sizeof(struct v29tx_cfg), 0x1c, 0);
	diff_eq_int("sizeof V29TX_CFG (%ld)", (long)sizeof(V29TX_CFG),
		    0x1c, 0);
	diff_eq_int("sizeof(struct v29tx) (%ld)", (long)sizeof(struct v29tx),
		    0x9c, 0);
	diff_eq_int("sizeof(struct fpm_sdm) (%ld)",
		    (long)sizeof(struct fpm_sdm), 0x18, 0);

	return diff_end();
}

/*
 * Everything `V29TX_create` builds, laid out to the private block's shallow
 * fields.
 */
static void
compare_tree(const char *what, void *a, void *b, long tag)
{
	void *pa = FIELD_PTR(a, V29TX_OBJ_PARAMS);
	void *pb = FIELD_PTR(b, V29TX_OBJ_PARAMS);
	struct v29tx *ta = V29TX(a);
	struct v29tx *tb = V29TX(b);
	struct fax_fifo *fa, *fb;
	struct sgd *sa, *sb;
	char buf[160];

	diff_eq_int("params allocated on both sides (%ld)",
		    pa != 0 && pb != 0, 1, tag);
	diff_eq_int("tx block allocated on both sides (%ld)",
		    ta != 0 && tb != 0, 1, tag);
	if (pa == 0 || pb == 0 || ta == 0 || tb == 0)
		return;

	/* The config, the handle's first 28 bytes. */
	snprintf(buf, sizeof(buf), "%.90s config bytes (%%ld)", what);
	diff_eq_int(buf, memcmp(a, b, sizeof(struct v29tx_cfg)) == 0, 1, tag);

	/* The result word: status, and the two flag bytes. */
	snprintf(buf, sizeof(buf), "%.90s result status (%%ld)", what);
	diff_eq_int(buf, *FIELD(a, V29TX_OBJ_RESULT),
		    *FIELD(b, V29TX_OBJ_RESULT), tag);
	snprintf(buf, sizeof(buf), "%.90s result B1 (%%ld)", what);
	diff_eq_int(buf, *FIELD(a, V29TX_OBJ_RESULT_B1),
		    *FIELD(b, V29TX_OBJ_RESULT_B1), tag);
	snprintf(buf, sizeof(buf), "%.90s result B2 (%%ld)", what);
	diff_eq_int(buf, *FIELD(a, V29TX_OBJ_RESULT_B2),
		    *FIELD(b, V29TX_OBJ_RESULT_B2), tag);

	/* The parameter/half-duplex block. */
	snprintf(buf, sizeof(buf), "%.90s params.state (%%ld)", what);
	diff_eq_int(buf, AT_S(pa, V29TXP_STATE), AT_S(pb, V29TXP_STATE), tag);
	snprintf(buf, sizeof(buf), "%.90s params.short_0016 (%%ld)", what);
	diff_eq_int(buf, AT_S(pa, V29TXP_SHORT_0016),
		    AT_S(pb, V29TXP_SHORT_0016), tag);
	snprintf(buf, sizeof(buf), "%.90s params.rate (%%ld)", what);
	diff_eq_int(buf, AT_S(pa, V29TXP_RATE), AT_S(pb, V29TXP_RATE), tag);
	snprintf(buf, sizeof(buf), "%.90s params.int_0008 (%%ld)", what);
	diff_eq_int(buf, AT_I(pa, V29TXP_INT_0008), AT_I(pb, V29TXP_INT_0008),
		    tag);
	snprintf(buf, sizeof(buf), "%.90s params.process, ours vs blob's (%%ld)",
		 what);
	diff_eq_int(buf, fn_id(FIELD_PTR(pa, V29TXP_PROCESS)),
		    fn_id(FIELD_PTR(pb, V29TXP_PROCESS)), tag);
	snprintf(buf, sizeof(buf), "%.90s params.scram_sr (%%ld)", what);
	diff_eq_int(buf, AT_S(pa, V29SCRAM_SR), AT_S(pb, V29SCRAM_SR), tag);

	fa = (struct fax_fifo *)FIELD_PTR(pa, V29TXP_FIFO);
	fb = (struct fax_fifo *)FIELD_PTR(pb, V29TXP_FIFO);
	diff_eq_int("FIFO built on both sides (%ld)", fa != 0 && fb != 0,
		    1, tag);
	if (fa != 0 && fb != 0) {
		diff_eq_int("fifo.size (%ld)", fa->size, fb->size, tag);
		diff_eq_int("fifo.fill (%ld)", fa->fill, fb->fill, tag);
		diff_eq_int("fifo.count (%ld)", fa->count, fb->count, tag);
		diff_eq_int("fifo.rd (%ld)", fa->rd, fb->rd, tag);
		diff_eq_int("fifo.wr (%ld)", fa->wr, fb->wr, tag);
	}

	sa = (struct sgd *)FIELD_PTR(pa, V29TXP_SGD);
	sb = (struct sgd *)FIELD_PTR(pb, V29TXP_SGD);
	diff_eq_int("SGD built on both sides (%ld)", sa != 0 && sb != 0,
		    1, tag);
	if (sa != 0 && sb != 0) {
		diff_eq_int("sgd.cfg.sym_bits (%ld)", sa->cfg.sym_bits,
			    sb->cfg.sym_bits, tag);
		diff_eq_int("sgd.cfg.sym_bits is 4 (%ld)", sa->cfg.sym_bits,
			    4, tag);
		diff_eq_int("sgd.cfg.hist_len (%ld)", sa->cfg.hist_len,
			    sb->cfg.hist_len, tag);
		/* D1291: gen.seq/seq_len/short_0006/seq_enable are excluded
		 * -- neither side's V29TX_create sets them and the object
		 * does not either. */
	}

	/* The private block's ring: len, and the rails' contents. */
	diff_eq_int("tx.ring.len (%ld)", ta->ring.len, tb->ring.len, tag);
	diff_eq_int("tx.ring.widx (%ld)", ta->ring.widx, tb->ring.widx, tag);
	diff_eq_int("tx.ring.ridx (%ld)", ta->ring.ridx, tb->ring.ridx, tag);
	if (ta->ring.len == tb->ring.len && ta->ring.len > 0
	    && ta->ring.len <= 0x32) {
		cmp_shorts("tx.ring.i[%ld]", ta->ring.i, tb->ring.i,
			   ta->ring.len);
		cmp_shorts("tx.ring.q[%ld]", ta->ring.q, tb->ring.q,
			   ta->ring.len);
	}

	/* The scrambler. */
	diff_eq_int("tx.sdm.cfg.nbits (%ld)", ta->sdm.cfg.nbits,
		    tb->sdm.cfg.nbits, tag);
	diff_eq_int("tx.sdm.cfg.tap1 (%ld)", ta->sdm.cfg.tap1,
		    tb->sdm.cfg.tap1, tag);
	diff_eq_int("tx.sdm.cfg.tap2 (%ld)", ta->sdm.cfg.tap2,
		    tb->sdm.cfg.tap2, tag);
	diff_eq_int("tx.sdm.reg (%ld)", (long)ta->sdm.reg, (long)tb->sdm.reg,
		    tag);

	/* The symbol coder: the scalar fields, not the table pointers
	 * (which point at the same VALUES through different addresses on
	 * each side). */
	diff_eq_int("tx.smc.cfg.direct (%ld)", ta->smc.cfg.direct,
		    tb->smc.cfg.direct, tag);
	diff_eq_int("tx.smc.cfg.rot_step (%ld)", ta->smc.cfg.rot_step,
		    tb->smc.cfg.rot_step, tag);
	diff_eq_int("tx.smc.cfg.rot_mod (%ld)", ta->smc.cfg.rot_mod,
		    tb->smc.cfg.rot_mod, tag);
	diff_eq_int("tx.smc.cfg.amask (%ld)", ta->smc.cfg.amask,
		    tb->smc.cfg.amask, tag);
	diff_eq_int("tx.smc.cfg.pmask (%ld)", ta->smc.cfg.pmask,
		    tb->smc.cfg.pmask, tag);
	diff_eq_int("tx.smc.quad (%ld)", ta->smc.quad, tb->smc.quad, tag);
	diff_eq_int("tx.smc.acc (%ld)", ta->smc.acc, tb->smc.acc, tag);

	/* The pulse shaper. */
	diff_eq_int("tx.pps.cfg.scale (%ld)", ta->pps.cfg.scale,
		    tb->pps.cfg.scale, tag);
	diff_eq_int("tx.pps.cfg.mapped (%ld)", ta->pps.cfg.mapped,
		    tb->pps.cfg.mapped, tag);
	diff_eq_int("tx.pps.cfg.coeffs (%ld)", ta->pps.cfg.coeffs,
		    tb->pps.cfg.coeffs, tag);
	diff_eq_int("tx.pps.need (%ld)", ta->pps.need, tb->pps.need, tag);
	diff_eq_int("tx.pps.taps (%ld)", ta->pps.taps, tb->pps.taps, tag);
	if (ta->pps.cfg.coeff_i != 0 && tb->pps.cfg.coeff_i != 0)
		cmp_shorts("tx.pps.cfg.coeff_i[%ld]", ta->pps.cfg.coeff_i,
			   tb->pps.cfg.coeff_i, 120);
}

/* ------------------------------------------------------------------------- */

static const struct {
	const char *name;
	int use_default;
	short bitrate;
} cases[] = {
	{ "params NULL (V29TX_CFG)",              1, 0 },
	{ "explicit, bitrate == 7200",             0, V29_BPS_7200 },
	{ "explicit, bitrate == 9600",             0, V29_BPS_9600 },
	{ "explicit, bitrate == 1200 (default arm)", 0, 1200 },
};
#define NCASES ((long)(sizeof(cases) / sizeof(cases[0])))

static void
build_cfg(struct v29tx_cfg *c, long k)
{
	*c = V29TX_CFG;
	c->bitrate = cases[k].bitrate;
}

static int
test_create_self_allocating(void)
{
	long k;

	diff_begin("v29txcreate: V29TX_create, self-allocating");

	for (k = 0; k < NCASES; k++) {
		struct v29tx_cfg ca, cb;
		void *a, *b;

		build_cfg(&ca, k);
		build_cfg(&cb, k);

		if (cases[k].use_default) {
			b = ref_V29TX_create(0, 0);
			a = V29TX_create(0, 0);
		} else {
			b = ref_V29TX_create(0, &cb);
			a = V29TX_create(0, &ca);
		}

		diff_eq_int("both built (%ld)", a != 0 && b != 0, 1, k);
		if (a == 0 || b == 0)
			continue;

		compare_tree(cases[k].name, a, b, k);

		{
			void *pa = FIELD_PTR(a, V29TX_OBJ_PARAMS);

			diff_eq_int("fresh: params.state is V29TX_STATE_START (%ld)",
				    AT_S(pa, V29TXP_STATE), V29TX_STATE_START,
				    k);
			diff_eq_int("fresh: params.process is TxHdxStartV29 (%ld)",
				    fn_id(FIELD_PTR(pa, V29TXP_PROCESS)), 0, k);
		}

		if (!cases[k].use_default && cases[k].bitrate != V29_BPS_7200
		    && cases[k].bitrate != V29_BPS_9600) {
			diff_eq_int("default-arm status is V29TX_STATUS_DEFAULT (%ld)",
				    *FIELD(a, V29TX_OBJ_RESULT),
				    V29TX_STATUS_DEFAULT, k);
			diff_eq_int("default-arm sets RESULT_B1_BIT1 (%ld)",
				    (*FIELD(a, V29TX_OBJ_RESULT_B1)
				     & V29TX_RESULT_B1_BIT1) != 0, 1, k);
			diff_eq_int("default-arm rate is V29_RATE_9600 (%ld)",
				    AT_S(FIELD_PTR(a, V29TX_OBJ_PARAMS),
					 V29TXP_RATE),
				    V29_RATE_9600, k);
		}

		V29TX_delete(a);
		ref_V29TX_delete(b);
	}

	return diff_end();
}

/*
 * Re-initialisation in place, D955/F8587 planted: the whole handle, the
 * FIFO's buffer and both rings are overwritten with a non-zero pattern
 * between two calls (keeping only the sub-pointers the constructor tests to
 * decide whether to allocate), so a field the second call forgets to
 * rewrite is visibly the pattern rather than invisibly a zero the first
 * call already left.
 */
static int
test_reinit(void)
{
	void *a, *b;
	void *pa, *pb;
	void *txa, *txb;
	struct v29tx_cfg ca, cb;

	diff_begin("v29txcreate: V29TX_create, re-initialised in place");

	ca = V29TX_CFG;
	cb = V29TX_CFG;
	a = V29TX_create(0, &ca);
	b = ref_V29TX_create(0, &cb);
	diff_eq_int("both built (%ld)", a != 0 && b != 0, 1, 0);
	if (a == 0 || b == 0)
		return diff_end();

	pa = FIELD_PTR(a, V29TX_OBJ_PARAMS);
	pb = FIELD_PTR(b, V29TX_OBJ_PARAMS);
	txa = V29TX(a);
	txb = V29TX(b);

	memset(a, 0x5a, 0x28);
	memset(b, 0x5a, 0x28);
	FIELD_PTR(a, V29TX_OBJ_PARAMS) = pa;
	FIELD_PTR(b, V29TX_OBJ_PARAMS) = pb;
	V29TX(a) = txa;
	V29TX(b) = txb;

	a = V29TX_create(a, &ca);
	b = ref_V29TX_create(b, &cb);

	diff_eq_int("params kept, ours (%ld)",
		    FIELD_PTR(a, V29TX_OBJ_PARAMS) == pa, 1, 0);
	diff_eq_int("params kept, blob's (%ld)",
		    FIELD_PTR(b, V29TX_OBJ_PARAMS) == pb, 1, 0);
	diff_eq_int("tx block kept, ours (%ld)", V29TX(a) == txa, 1, 0);
	diff_eq_int("tx block kept, blob's (%ld)", V29TX(b) == txb, 1, 0);

	compare_tree("reinit", a, b, 0);

	diff_eq_int("reinit: params.state is V29TX_STATE_START (%ld)",
		    AT_S(pa, V29TXP_STATE), V29TX_STATE_START, 0);
	diff_eq_int("reinit: params.process is TxHdxStartV29 (%ld)",
		    fn_id(FIELD_PTR(pa, V29TXP_PROCESS)), 0, 0);

	V29TX_delete(a);
	ref_V29TX_delete(b);

	return diff_end();
}

/*
 * The half-duplex machine through `V29TX_modem`, over many consecutive
 * blocks. `queue` feeds the transmit FIFO by hand -- there is no
 * reconstructed producer for it -- so a block sometimes has data queued and
 * sometimes does not, which is what carries the machine all the way around
 * its seven-state cycle: enough blocks pass that every one of
 * START/QUIET/ALT/EQCOND/SCR1/DATA/IDLE installs at least once on both
 * sides.
 */
static unsigned
rnd(unsigned *s)
{
	*s = (*s) * 1103515245u + 12345u;
	return (*s >> 16) & 0x7fffu;
}

static int
test_tx_cycle(void)
{
	void *a, *b;
	struct v29tx_cfg ca, cb;
	short outa[8192], outb[8192];
	unsigned seed = 292929u;
	int block;
	int seen_state[7];
	int i;

	diff_begin("v29txcreate: the half-duplex machine through V29TX_modem");

	ca = V29TX_CFG;
	cb = V29TX_CFG;
	ca.bitrate = cb.bitrate = V29_BPS_9600;
	a = V29TX_create(0, &ca);
	b = ref_V29TX_create(0, &cb);
	diff_eq_int("both built (%ld)", a != 0 && b != 0, 1, 0);
	if (a == 0 || b == 0)
		return diff_end();

	for (i = 0; i < 7; i++)
		seen_state[i] = 0;

	for (block = 0; block < 400; block++) {
		/*
		 * `in` is reused as SCRATCH by the whole V29TXP_PROCESS
		 * dispatch loop, not just for the initial FIFO_write --
		 * TxHdxEQCondV29/TxHdxABV29/TxHdxSCR1V29 write up to
		 * V29TX_MODEM_BUDGET (0x30) elements into it directly, and
		 * TxHdxDataV29's FIFO_read can too.  Sized for the whole
		 * budget, not just the queued count -- an 8-element buffer
		 * here is a stack overflow, not a failing comparison.
		 */
		unsigned short qbuf_a[V29TX_MODEM_BUDGET], qbuf_b[V29TX_MODEM_BUDGET];
		int qn = (int)(rnd(&seed) % 9);
		unsigned short counta, countb;
		int reta, retb;
		void *pa;
		short st;

		for (i = 0; i < qn; i++)
			qbuf_a[i] = qbuf_b[i] =
				(unsigned short)(rnd(&seed) & 1);

		counta = countb = (unsigned short)qn;
		memset(outa, 0xa5, sizeof outa);
		memset(outb, 0xa5, sizeof outb);

		reta = V29TX_modem(a, qbuf_a, outa, &counta);
		retb = ref_V29TX_modem(b, qbuf_b, outb, &countb);

		diff_eq_int("V29TX_modem return (%ld)", reta, retb, block);
		diff_eq_int("V29TX_modem *count (%ld)", counta, countb, block);
		if (counta == countb && counta <= 8192)
			cmp_ushorts("out[%ld]", (unsigned short *)outa,
				    (unsigned short *)outb, counta);

		pa = FIELD_PTR(a, V29TX_OBJ_PARAMS);
		st = AT_S(pa, V29TXP_STATE);
		if (st >= 0 && st < 7)
			seen_state[st] = 1;
	}

	/*
	 * START (0) and IDLE-just-installed (6) are entered and left again
	 * within the SAME V29TX_modem call (each spends a 0-block budget
	 * before advancing, and the do/while loop keeps running while
	 * `budget > 0`) -- sampling `params.state` only once per call, after
	 * it returns, can miss them landing there transiently.  QUIET/ALT/
	 * EQCOND/SCR1/DATA (1..5) each hold the machine for at least one
	 * whole V29TX_modem call's budget, so those five are checked.
	 */
	for (i = 1; i <= 5; i++)
		diff_eq_int("state %ld visited during the run", seen_state[i],
			    1, i);

	compare_tree("after modem cycle", a, b, 0);

	V29TX_delete(a);
	ref_V29TX_delete(b);

	return diff_end();
}

/*
 * The one arm `V29TX_modem` alone cannot reliably reach: `TxHdxDataV29`'s
 * `V29TXP_INT_0008 != 0` underrun arm, which leaves the remainder in
 * `*budget` and calls `TxNextStateV29` rather than consuming the whole
 * request.  Reached by calling `TxHdxDataV29` directly against a handle
 * `V29TX_create` built, with the FIFO underfed and the field poked by
 * offset.
 */
static int
test_data_underrun_bypass_arm(void)
{
	void *a, *b;
	struct v29tx_cfg ca, cb;
	void *pa, *pb;
	struct fax_fifo *fa, *fb;
	unsigned short ina[8], inb[8], qa[2], qb[2];
	short outa[512], outb[512];
	short budgeta, budgetb;
	short reta, retb;

	diff_begin("v29txcreate: TxHdxDataV29's V29TXP_INT_0008 != 0 underrun arm");

	ca = V29TX_CFG;
	cb = V29TX_CFG;
	a = V29TX_create(0, &ca);
	b = ref_V29TX_create(0, &cb);
	diff_eq_int("both built (%ld)", a != 0 && b != 0, 1, 0);
	if (a == 0 || b == 0)
		return diff_end();

	pa = FIELD_PTR(a, V29TX_OBJ_PARAMS);
	pb = FIELD_PTR(b, V29TX_OBJ_PARAMS);
	fa = (struct fax_fifo *)FIELD_PTR(pa, V29TXP_FIFO);
	fb = (struct fax_fifo *)FIELD_PTR(pb, V29TXP_FIFO);

	/* Queue 2 elements, ask TxHdxDataV29 for 8: an underrun. */
	qa[0] = qa[1] = 1;
	qb[0] = qb[1] = 1;
	FIFO_write(fa, qa, 2);
	FIFO_write(fb, qb, 2);

	AT_I(pa, V29TXP_INT_0008) = 1;
	AT_I(pb, V29TXP_INT_0008) = 1;
	AT_S(pa, V29TXP_STATE) = V29TX_STATE_DATA;
	AT_S(pb, V29TXP_STATE) = V29TX_STATE_DATA;
	AT_S(pa, V29TXP_SHORT_0016) = 0;
	AT_S(pb, V29TXP_SHORT_0016) = 0;

	memset(ina, 0xa5, sizeof ina);
	memset(inb, 0xa5, sizeof inb);
	memset(outa, 0xa5, sizeof outa);
	memset(outb, 0xa5, sizeof outb);
	budgeta = budgetb = 8;

	reta = TxHdxDataV29(a, ina, outa, &budgeta);
	retb = ref_TxHdxDataV29(b, inb, outb, &budgetb);

	diff_eq_int("return (%ld)", reta, retb, 0);
	diff_eq_int("*budget LEFT NON-ZERO (%ld)", budgeta, budgetb, 0);
	diff_eq_int("*budget is 6 (8 requested - 2 taken) (%ld)", budgeta,
		    6, 0);
	cmp_shorts("out[%ld]", outa, outb, reta < retb ? reta : retb);
	diff_eq_int("params.state advanced, ours (%ld)",
		    fn_id(FIELD_PTR(pa, V29TXP_PROCESS)), 6, 0);
	diff_eq_int("params.state advanced, blob's (%ld)",
		    fn_id(FIELD_PTR(pb, V29TXP_PROCESS)), 6, 0);
	diff_eq_int("params.state is V29TX_STATE_IDLE, ours (%ld)",
		    AT_S(pa, V29TXP_STATE), V29TX_STATE_IDLE, 0);

	V29TX_delete(a);
	ref_V29TX_delete(b);

	return diff_end();
}

/*
 * `TxNextStateV29`'s out-of-range default arm, `t_v21txcreate.c`'s idiom.
 */
static int
test_next_state_default_arm(void)
{
	void *a, *b;
	struct v29tx_cfg ca, cb;
	void *pa, *pb;
	short states[3] = { 7, -5, 100 };
	int i;

	diff_begin("v29txcreate: TxNextStateV29's out-of-range default arm");

	ca = V29TX_CFG;
	cb = V29TX_CFG;
	a = V29TX_create(0, &ca);
	b = ref_V29TX_create(0, &cb);
	diff_eq_int("both built (%ld)", a != 0 && b != 0, 1, 0);
	if (a == 0 || b == 0)
		return diff_end();

	pa = FIELD_PTR(a, V29TX_OBJ_PARAMS);
	pb = FIELD_PTR(b, V29TX_OBJ_PARAMS);

	for (i = 0; i < 3; i++) {
		AT_S(pa, V29TXP_STATE) = states[i];
		AT_S(pb, V29TXP_STATE) = states[i];
		*FIELD(a, V29TX_OBJ_RESULT_B1) = 0;
		*FIELD(b, V29TX_OBJ_RESULT_B1) = 0;
		*FIELD(a, V29TX_OBJ_RESULT_B2) = 0xff;
		*FIELD(b, V29TX_OBJ_RESULT_B2) = 0xff;

		TxNextStateV29(a);
		ref_TxNextStateV29(b);

		diff_eq_int("state unchanged, ours (%ld)",
			    AT_S(pa, V29TXP_STATE), states[i], i);
		diff_eq_int("state unchanged, blob's (%ld)",
			    AT_S(pb, V29TXP_STATE), states[i], i);
		diff_eq_int("status is V29TX_STATUS_DEFAULT (%ld)",
			    *FIELD(a, V29TX_OBJ_RESULT), V29TX_STATUS_DEFAULT,
			    i);
		diff_eq_int("RESULT_B1 (%ld)", *FIELD(a, V29TX_OBJ_RESULT_B1),
			    *FIELD(b, V29TX_OBJ_RESULT_B1), i);
		diff_eq_int("RESULT_B1_BIT1 set (%ld)",
			    (*FIELD(a, V29TX_OBJ_RESULT_B1)
			     & V29TX_RESULT_B1_BIT1) != 0, 1, i);
		diff_eq_int("RESULT_B2 (%ld)", *FIELD(a, V29TX_OBJ_RESULT_B2),
			    *FIELD(b, V29TX_OBJ_RESULT_B2), i);
		diff_eq_int("RESULT_B2_BIT0 cleared (%ld)",
			    (*FIELD(a, V29TX_OBJ_RESULT_B2)
			     & V29TX_RESULT_B2_BIT0) != 0, 0, i);
	}

	V29TX_delete(a);
	ref_V29TX_delete(b);

	return diff_end();
}

/* ------------------------------------------------------------------------- */

int
main(void)
{
	int rc = 0;

	rc |= test_shape();
	rc |= test_create_self_allocating();
	rc |= test_reinit();
	rc |= test_tx_cycle();
	rc |= test_data_underrun_bypass_arm();
	rc |= test_next_state_default_arm();

	return rc;
}

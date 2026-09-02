/*
 * t_v17txcreate.c -- differential test of `V17TX_create` (.text 0x0989e0,
 *                    1,043 bytes) and the transmit half-duplex machine it
 *                    installs: `TxNextStateV17` and the nine `TxHdx*V17`
 *                    states -- twelve symbols, 4,145 bytes, and per finding
 *                    F9600/F9912 they link only together.
 *
 * `t_v29txcreate.c`'s own shape, one modulation over and TWELVE states
 * instead of seven -- see `TxNextStateV17`'s own comment in `src/fax/v17.c`
 * and the state table in `include/dsplib/v17fax.h` for the state numbering
 * and which handler each installs.
 *
 *   1. WIRING.  `V17TX_create` self-allocating over every recognised bit
 *      rate and the unrecognised-rate default arm, comparing the handle's
 *      config bytes, the parameter block and the private TX block's shallow
 *      fields against the blob.
 *
 *   2. RE-INITIALISATION IN PLACE, D955/F8587 planted.
 *
 *   3. THE HALF-DUPLEX MACHINE THROUGH `V17TX_modem`, over many consecutive
 *      blocks with the FIFO sometimes fed and sometimes left to run dry --
 *      long training (`V17TXP_INT_000C` == 0), which is what drives the
 *      machine through START -> SILENCE -> TEP -> QUIET -> ALT -> EQCOND ->
 *      BRIDGE -> SCR1 -> DATA -> SCR1_END -> QUIET_END -> IDLE -> START.
 *
 *   4. `EQCOND`'S OTHER ARM: short training (`V17TXP_INT_000C` != 0), which
 *      installs `TxHdxSCR1V17` directly and never visits BRIDGE -- driven
 *      the same way, over fewer blocks because ALT's own budget is short
 *      under this arm too (0x26 against 0xba0).
 *
 *   5. THE ARMS THE FIFO-DRIVEN CYCLES CANNOT REACH RELIABLY:
 *      `TxHdxDataV17`'s underrun-with-`V17TXP_INT_0008`-set arm (nothing
 *      reconstructed ever sets that field non-zero) and
 *      `TxNextStateV17`'s out-of-range default arm.  Both reached by
 *      calling the half-duplex functions directly against a handle
 *      `V17TX_create` built, `t_v29txcreate.c`'s own idiom.
 */

#include <stddef.h>
#include <stdio.h>
#include <string.h>

#include "harness.h"

#include "dsplib/faxfifo.h"
#include "dsplib/fpm_pps.h"
#include "dsplib/fpm_sdm.h"
#include "dsplib/fpm_smc.h"
#include "dsplib/sdm.h"
#include "dsplib/sgd.h"
#include "dsplib/v17data.h"
#include "dsplib/v17fax.h"

extern void *ref_V17TX_create(void *modem, const struct v17tx_cfg *params);
extern void ref_V17TX_delete(void *modem);
extern int ref_V17TX_modem(void *modem, unsigned short *in, short *out,
			   unsigned short *count);
extern short ref_TxHdxStartV17(void *modem, unsigned short *in, short *out,
			       short *budget);
extern short ref_TxHdxSilenceV17(void *modem, unsigned short *in, short *out,
				 short *budget);
extern short ref_TxHdxTEP_V17(void *modem, unsigned short *in, short *out,
			      short *budget);
extern short ref_TxHdxABV17(void *modem, unsigned short *in, short *out,
			    short *budget);
extern short ref_TxHdxEQCondV17(void *modem, unsigned short *in, short *out,
				short *budget);
extern short ref_TxHdxBridgeV17(void *modem, unsigned short *in, short *out,
				short *budget);
extern short ref_TxHdxSCR1V17(void *modem, unsigned short *in, short *out,
			      short *budget);
extern short ref_TxHdxDataV17(void *modem, unsigned short *in, short *out,
			      short *budget);
extern short ref_TxHdxIdleV17(void *modem, unsigned short *in, short *out,
			      short *budget);
extern void ref_TxNextStateV17(void *modem);
extern struct v17tx_cfg ref_V17TX_CFG;

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

/* Which of the nine transmit handlers a pointer is, on EITHER side. */
static int
fn_id(const void *p)
{
	if (p == (const void *)TxHdxStartV17
	    || p == (const void *)&ref_TxHdxStartV17)
		return 0;
	if (p == (const void *)TxHdxSilenceV17
	    || p == (const void *)&ref_TxHdxSilenceV17)
		return 1;
	if (p == (const void *)TxHdxTEP_V17
	    || p == (const void *)&ref_TxHdxTEP_V17)
		return 2;
	if (p == (const void *)TxHdxABV17
	    || p == (const void *)&ref_TxHdxABV17)
		return 3;
	if (p == (const void *)TxHdxEQCondV17
	    || p == (const void *)&ref_TxHdxEQCondV17)
		return 4;
	if (p == (const void *)TxHdxBridgeV17
	    || p == (const void *)&ref_TxHdxBridgeV17)
		return 5;
	if (p == (const void *)TxHdxSCR1V17
	    || p == (const void *)&ref_TxHdxSCR1V17)
		return 6;
	if (p == (const void *)TxHdxDataV17
	    || p == (const void *)&ref_TxHdxDataV17)
		return 7;
	if (p == (const void *)TxHdxIdleV17
	    || p == (const void *)&ref_TxHdxIdleV17)
		return 8;
	return -1;
}

/* ------------------------------------------------------------------------- */

static int
test_shape(void)
{
	diff_begin("v17txcreate: sizes against the object's symbol table");

	diff_eq_int("sizeof(struct v17tx_cfg) (%ld)",
		    (long)sizeof(struct v17tx_cfg), 0x20, 0);
	diff_eq_int("sizeof V17TX_CFG (%ld)", (long)sizeof(V17TX_CFG),
		    0x20, 0);

	return diff_end();
}

/*
 * Everything `V17TX_create` builds, laid out to the private block's shallow
 * fields.
 */
static void
compare_tree(const char *what, void *a, void *b, long tag)
{
	void *pa = FIELD_PTR(a, V17TX_OBJ_PARAMS);
	void *pb = FIELD_PTR(b, V17TX_OBJ_PARAMS);
	void *fa = FIELD_PTR(a, V17TX_OBJ_FP);
	void *fb = FIELD_PTR(b, V17TX_OBJ_FP);
	struct fax_fifo *fifoa, *fifob;
	struct sgd *sa, *sb;
	struct fpm_smc_ring *ra, *rb;
	struct fpm_sdm *da, *db;
	struct fpm_pps *ppa, *ppb;
	char buf[160];

	diff_eq_int("params allocated on both sides (%ld)",
		    pa != 0 && pb != 0, 1, tag);
	diff_eq_int("private block allocated on both sides (%ld)",
		    fa != 0 && fb != 0, 1, tag);
	if (pa == 0 || pb == 0 || fa == 0 || fb == 0)
		return;

	/* The config, the handle's first 0x20 bytes. */
	snprintf(buf, sizeof(buf), "%.90s config bytes (%%ld)", what);
	diff_eq_int(buf, memcmp(a, b, sizeof(struct v17tx_cfg)) == 0, 1, tag);

	/* The result word: status, and the two flag bytes. */
	snprintf(buf, sizeof(buf), "%.90s result status (%%ld)", what);
	diff_eq_int(buf, *FIELD(a, V17TX_OBJ_RESULT),
		    *FIELD(b, V17TX_OBJ_RESULT), tag);
	snprintf(buf, sizeof(buf), "%.90s result B1 (%%ld)", what);
	diff_eq_int(buf, *FIELD(a, V17TX_OBJ_RESULT_B1),
		    *FIELD(b, V17TX_OBJ_RESULT_B1), tag);
	snprintf(buf, sizeof(buf), "%.90s result B2 (%%ld)", what);
	diff_eq_int(buf, *FIELD(a, V17TX_OBJ_RESULT_B2),
		    *FIELD(b, V17TX_OBJ_RESULT_B2), tag);

	/* The parameter/half-duplex block. */
	snprintf(buf, sizeof(buf), "%.90s params.state (%%ld)", what);
	diff_eq_int(buf, AT_S(pa, V17TXP_STATE), AT_S(pb, V17TXP_STATE), tag);
	snprintf(buf, sizeof(buf), "%.90s params.short_001a (%%ld)", what);
	diff_eq_int(buf, AT_S(pa, V17TXP_SHORT_001A),
		    AT_S(pb, V17TXP_SHORT_001A), tag);
	snprintf(buf, sizeof(buf), "%.90s params.mode (%%ld)", what);
	diff_eq_int(buf, AT_S(pa, V17TXP_MODE), AT_S(pb, V17TXP_MODE), tag);
	snprintf(buf, sizeof(buf), "%.90s params.int_0008 (%%ld)", what);
	diff_eq_int(buf, AT_I(pa, V17TXP_INT_0008), AT_I(pb, V17TXP_INT_0008),
		    tag);
	snprintf(buf, sizeof(buf), "%.90s params.int_000c (%%ld)", what);
	diff_eq_int(buf, AT_I(pa, V17TXP_INT_000C), AT_I(pb, V17TXP_INT_000C),
		    tag);
	snprintf(buf, sizeof(buf), "%.90s params.nocarrier_sym (%%ld)", what);
	diff_eq_int(buf, AT_S(pa, V17TXP_NOCARRIER_SYM),
		    AT_S(pb, V17TXP_NOCARRIER_SYM), tag);
	snprintf(buf, sizeof(buf), "%.90s params.process, ours vs blob's (%%ld)",
		 what);
	diff_eq_int(buf, fn_id(FIELD_PTR(pa, V17TXP_PROCESS)),
		    fn_id(FIELD_PTR(pb, V17TXP_PROCESS)), tag);

	fifoa = (struct fax_fifo *)FIELD_PTR(pa, V17TXP_FIFO);
	fifob = (struct fax_fifo *)FIELD_PTR(pb, V17TXP_FIFO);
	diff_eq_int("FIFO built on both sides (%ld)",
		    fifoa != 0 && fifob != 0, 1, tag);
	if (fifoa != 0 && fifob != 0) {
		diff_eq_int("fifo.size (%ld)", fifoa->size, fifob->size, tag);
		diff_eq_int("fifo.fill (%ld)", fifoa->fill, fifob->fill, tag);
		diff_eq_int("fifo.count (%ld)", fifoa->count, fifob->count,
			    tag);
		diff_eq_int("fifo.rd (%ld)", fifoa->rd, fifob->rd, tag);
		diff_eq_int("fifo.wr (%ld)", fifoa->wr, fifob->wr, tag);
	}

	sa = (struct sgd *)FIELD_PTR(pa, V17TXP_SGD);
	sb = (struct sgd *)FIELD_PTR(pb, V17TXP_SGD);
	diff_eq_int("SGD built on both sides (%ld)", sa != 0 && sb != 0,
		    1, tag);
	if (sa != 0 && sb != 0) {
		/*
		 * sym_bits is 2 only until the machine's own `BRIDGE`/
		 * `EQCOND` arms call `SetTxModeV17`, which RECONFIGURES the
		 * same SGD object with `V17TX_SYM_SIZE[mode]` (SetTxModeV17's
		 * own comment) -- so the literal-2 assertion belongs only at
		 * fresh construction, not in this cross-side check, which a
		 * mid-cycle caller also uses.  See `test_create_self_
		 * allocating`/`test_reinit` for the construction-time check.
		 */
		diff_eq_int("sgd.cfg.sym_bits (%ld)", sa->cfg.sym_bits,
			    sb->cfg.sym_bits, tag);
		diff_eq_int("sgd.cfg.hist_len (%ld)", sa->cfg.hist_len,
			    sb->cfg.hist_len, tag);
	}

	/* The private block's ring. */
	ra = (struct fpm_smc_ring *)(void *)FIELD(fa, V17FP_SMC_RING);
	rb = (struct fpm_smc_ring *)(void *)FIELD(fb, V17FP_SMC_RING);
	diff_eq_int("ring.len (%ld)", ra->len, rb->len, tag);
	diff_eq_int("ring.widx (%ld)", ra->widx, rb->widx, tag);
	diff_eq_int("ring.ridx (%ld)", ra->ridx, rb->ridx, tag);
	diff_eq_int("ring.i is NULL, ours (%ld)", ra->i == 0, 1, tag);
	diff_eq_int("ring.q is NULL, ours (%ld)", ra->q == 0, 1, tag);
	if (ra->len == rb->len && ra->len > 0 && ra->len <= 0x32
	    && ra->sym != 0 && rb->sym != 0)
		cmp_shorts("ring.sym[%ld]", ra->sym, rb->sym, ra->len);

	/* The scrambler. */
	da = (struct fpm_sdm *)(void *)FIELD(fa, V17FP_SDM);
	db = (struct fpm_sdm *)(void *)FIELD(fb, V17FP_SDM);
	diff_eq_int("sdm.cfg.nbits (%ld)", da->cfg.nbits, db->cfg.nbits, tag);
	diff_eq_int("sdm.cfg.tap1 (%ld)", da->cfg.tap1, db->cfg.tap1, tag);
	diff_eq_int("sdm.cfg.tap2 (%ld)", da->cfg.tap2, db->cfg.tap2, tag);
	diff_eq_int("sdm.reg (%ld)", (long)da->reg, (long)db->reg, tag);

	/* The pulse shaper. */
	ppa = (struct fpm_pps *)(void *)FIELD(fa, V17FP_PPS);
	ppb = (struct fpm_pps *)(void *)FIELD(fb, V17FP_PPS);
	diff_eq_int("pps.cfg.scale (%ld)", ppa->cfg.scale, ppb->cfg.scale,
		    tag);
	diff_eq_int("pps.cfg.mapped (%ld)", ppa->cfg.mapped, ppb->cfg.mapped,
		    tag);
	diff_eq_int("pps.cfg.coeffs (%ld)", ppa->cfg.coeffs, ppb->cfg.coeffs,
		    tag);
	diff_eq_int("pps.need (%ld)", ppa->need, ppb->need, tag);
	if (ppa->cfg.coeff_i != 0 && ppb->cfg.coeff_i != 0)
		cmp_shorts("pps.cfg.coeff_i[%ld]", ppa->cfg.coeff_i,
			   ppb->cfg.coeff_i, 120);
}

/* ------------------------------------------------------------------------- */

static const struct {
	const char *name;
	int use_default;
	short bitrate;
} cases[] = {
	{ "params NULL (V17TX_CFG)",                1, 0     },
	{ "explicit, bitrate == 7200",               0, 7200  },
	{ "explicit, bitrate == 9600",               0, 9600  },
	{ "explicit, bitrate == 12000",              0, 12000 },
	{ "explicit, bitrate == 14400",              0, 14400 },
	{ "explicit, bitrate == 2400 (default arm)", 0, 2400  },
};
#define NCASES ((long)(sizeof(cases) / sizeof(cases[0])))

static void
build_cfg(struct v17tx_cfg *c, long k)
{
	*c = V17TX_CFG;
	c->bitrate = cases[k].bitrate;
}

static int
test_create_self_allocating(void)
{
	long k;

	diff_begin("v17txcreate: V17TX_create, self-allocating");

	for (k = 0; k < NCASES; k++) {
		struct v17tx_cfg ca, cb;
		void *a, *b;

		build_cfg(&ca, k);
		build_cfg(&cb, k);

		if (cases[k].use_default) {
			b = ref_V17TX_create(0, 0);
			a = V17TX_create(0, 0);
		} else {
			b = ref_V17TX_create(0, &cb);
			a = V17TX_create(0, &ca);
		}

		diff_eq_int("both built (%ld)", a != 0 && b != 0, 1, k);
		if (a == 0 || b == 0)
			continue;

		compare_tree(cases[k].name, a, b, k);

		{
			void *pa = FIELD_PTR(a, V17TX_OBJ_PARAMS);
			struct sgd *sa = (struct sgd *)
				FIELD_PTR(pa, V17TXP_SGD);

			diff_eq_int("fresh: params.state is V17TX_STATE_START (%ld)",
				    AT_S(pa, V17TXP_STATE), V17TX_STATE_START,
				    k);
			diff_eq_int("fresh: params.process is TxHdxStartV17 (%ld)",
				    fn_id(FIELD_PTR(pa, V17TXP_PROCESS)), 0, k);
			/*
			 * sgd.cfg.sym_bits is 2 ONLY at fresh construction --
			 * `SetTxModeV17`, called from `TxNextStateV17`'s
			 * BRIDGE/EQCOND arms, reconfigures the same object
			 * with `V17TX_SYM_SIZE[mode]` the moment the machine
			 * runs; see `compare_tree`'s own comment.
			 */
			if (sa != 0)
				diff_eq_int("fresh: sgd.cfg.sym_bits is 2 (%ld)",
					    sa->cfg.sym_bits, 2, k);
		}

		if (!cases[k].use_default && cases[k].bitrate != 7200
		    && cases[k].bitrate != 9600 && cases[k].bitrate != 12000
		    && cases[k].bitrate != 14400) {
			diff_eq_int("default-arm status is V17TX_RESULT_BYTE_07 (%ld)",
				    *FIELD(a, V17TX_OBJ_RESULT),
				    V17TX_RESULT_BYTE_07, k);
			diff_eq_int("default-arm sets RESULT_B1_BIT1 (%ld)",
				    (*FIELD(a, V17TX_OBJ_RESULT_B1)
				     & V17TX_RESULT_B1_BIT1) != 0, 1, k);
			diff_eq_int("default-arm mode is 3 (%ld)",
				    AT_S(FIELD_PTR(a, V17TX_OBJ_PARAMS),
					 V17TXP_MODE),
				    3, k);
		}

		V17TX_delete(a);
		ref_V17TX_delete(b);
	}

	return diff_end();
}

/*
 * Re-initialisation in place, D955/F8587 planted: the whole handle, the
 * FIFO's buffer and the ring's sym array are overwritten with a non-zero
 * pattern between two calls (keeping only the sub-pointers the constructor
 * tests to decide whether to allocate), so a field the second call forgets
 * to rewrite is visibly the pattern rather than invisibly a zero the first
 * call already left.
 */
static int
test_reinit(void)
{
	void *a, *b;
	void *pa, *pb;
	void *fa, *fb;
	struct v17tx_cfg ca, cb;

	diff_begin("v17txcreate: V17TX_create, re-initialised in place");

	ca = V17TX_CFG;
	cb = V17TX_CFG;
	a = V17TX_create(0, &ca);
	b = ref_V17TX_create(0, &cb);
	diff_eq_int("both built (%ld)", a != 0 && b != 0, 1, 0);
	if (a == 0 || b == 0)
		return diff_end();

	pa = FIELD_PTR(a, V17TX_OBJ_PARAMS);
	pb = FIELD_PTR(b, V17TX_OBJ_PARAMS);
	fa = FIELD_PTR(a, V17TX_OBJ_FP);
	fb = FIELD_PTR(b, V17TX_OBJ_FP);

	memset(a, 0x5a, 0x2c);
	memset(b, 0x5a, 0x2c);
	FIELD_PTR(a, V17TX_OBJ_PARAMS) = pa;
	FIELD_PTR(b, V17TX_OBJ_PARAMS) = pb;
	FIELD_PTR(a, V17TX_OBJ_FP) = fa;
	FIELD_PTR(b, V17TX_OBJ_FP) = fb;

	a = V17TX_create(a, &ca);
	b = ref_V17TX_create(b, &cb);

	diff_eq_int("params kept, ours (%ld)",
		    FIELD_PTR(a, V17TX_OBJ_PARAMS) == pa, 1, 0);
	diff_eq_int("params kept, blob's (%ld)",
		    FIELD_PTR(b, V17TX_OBJ_PARAMS) == pb, 1, 0);
	diff_eq_int("private block kept, ours (%ld)",
		    FIELD_PTR(a, V17TX_OBJ_FP) == fa, 1, 0);
	diff_eq_int("private block kept, blob's (%ld)",
		    FIELD_PTR(b, V17TX_OBJ_FP) == fb, 1, 0);

	compare_tree("reinit", a, b, 0);

	diff_eq_int("reinit: params.state is V17TX_STATE_START (%ld)",
		    AT_S(pa, V17TXP_STATE), V17TX_STATE_START, 0);
	diff_eq_int("reinit: params.process is TxHdxStartV17 (%ld)",
		    fn_id(FIELD_PTR(pa, V17TXP_PROCESS)), 0, 0);
	{
		struct sgd *sa = (struct sgd *)FIELD_PTR(pa, V17TXP_SGD);

		/* See test_create_self_allocating's own comment. */
		if (sa != 0)
			diff_eq_int("reinit: sgd.cfg.sym_bits is 2 (%ld)",
				    sa->cfg.sym_bits, 2, 0);
	}

	V17TX_delete(a);
	ref_V17TX_delete(b);

	return diff_end();
}

/*
 * The half-duplex machine through `V17TX_modem`, over many consecutive
 * blocks. `queue` feeds the transmit FIFO by hand -- there is no
 * reconstructed producer for it -- so a block sometimes has data queued and
 * sometimes does not, which is what carries the machine all the way around
 * its twelve-state cycle.  `short_train` selects `V17TXP_INT_000C`, which
 * governs both ALT's budget and whether EQCOND installs BRIDGE or SCR1
 * directly -- driven both ways so both of EQCOND's arms are covered, per
 * F134's "a detector must report its denominator" discipline: every state
 * visited is counted and the count is asserted, not merely hoped for.
 */
static unsigned
rnd(unsigned *s)
{
	*s = (*s) * 1103515245u + 12345u;
	return (*s >> 16) & 0x7fffu;
}

static int
run_tx_cycle(const char *label, int short_train, unsigned seed, int nblocks)
{
	void *a, *b;
	struct v17tx_cfg ca, cb;
	short outa[16384], outb[16384];
	int block;
	int seen_state[12];
	int i;
	int rc;

	diff_begin(label);

	ca = V17TX_CFG;
	cb = V17TX_CFG;
	ca.bitrate = cb.bitrate = 9600;
	ca.int_0018 = cb.int_0018 = short_train ? 1 : 0;
	a = V17TX_create(0, &ca);
	b = ref_V17TX_create(0, &cb);
	diff_eq_int("both built (%ld)", a != 0 && b != 0, 1, 0);
	if (a == 0 || b == 0)
		return diff_end();

	for (i = 0; i < 12; i++)
		seen_state[i] = 0;

	for (block = 0; block < nblocks; block++) {
		/*
		 * `in` is reused as SCRATCH by the whole `V17TXP_PROCESS`
		 * dispatch loop, not just for the initial `FIFO_write` --
		 * the training states write up to `V17TX_MODEM_BUDGET`
		 * (0x30) elements into it directly, and `TxHdxDataV17`'s
		 * `FIFO_read` can too.
		 */
		unsigned short qbuf_a[V17TX_MODEM_BUDGET],
			       qbuf_b[V17TX_MODEM_BUDGET];
		int qn = (int)(rnd(&seed) % 9);
		unsigned short counta, countb;
		int reta, retb;
		void *pa;
		short st;

		for (i = 0; i < qn; i++)
			qbuf_a[i] = qbuf_b[i] =
				(unsigned short)(rnd(&seed) & 3);

		counta = countb = (unsigned short)qn;
		memset(outa, 0xa5, sizeof outa);
		memset(outb, 0xa5, sizeof outb);

		reta = V17TX_modem(a, qbuf_a, outa, &counta);
		retb = ref_V17TX_modem(b, qbuf_b, outb, &countb);

		diff_eq_int("V17TX_modem return (%ld)", reta, retb, block);
		diff_eq_int("V17TX_modem *count (%ld)", counta, countb, block);
		if (counta == countb && counta <= 16384)
			cmp_ushorts("out[%ld]", (unsigned short *)outa,
				    (unsigned short *)outb, counta);

		pa = FIELD_PTR(a, V17TX_OBJ_PARAMS);
		st = AT_S(pa, V17TXP_STATE);
		if (st >= 0 && st < 12)
			seen_state[st] = 1;

		compare_tree("mid-cycle", a, b, block);
	}

	/*
	 * START (0), SCR1 (7), SCR1_END (9), QUIET_END (10) and
	 * IDLE-just-installed (11) are excluded from the "must visit"
	 * check, and for two different reasons.
	 *
	 * START/SCR1/IDLE spend a 0- or 1-block budget before advancing, and
	 * the do/while loop keeps running while `budget > 0`, so sampling
	 * `params.state` only once per call can miss them landing there
	 * transiently -- `t_v29txcreate.c`'s own reason for excluding its
	 * START/IDLE.
	 *
	 * SCR1_END/QUIET_END are UNREACHABLE by this loop at all: `DATA`'s
	 * own underrun-with-`V17TXP_INT_0008`-CLEAR arm (this test's FIFO is
	 * always underfed relative to the budget, and nothing here ever
	 * sets that field) reports `V17TX_STATUS_UNDERRUN` and returns
	 * WITHOUT calling `TxNextStateV17` -- so once entered, `DATA` never
	 * leaves under these conditions, the same reason `V29TX_create`'s
	 * own test excludes its IDLE (state 6) from the same check. Reached
	 * instead by `test_data_underrun_bypass_arm`, which drives
	 * `TxHdxDataV17` directly with `V17TXP_INT_0008` set.
	 *
	 * SILENCE/TEP/QUIET/ALT/EQCOND/BRIDGE/DATA each hold the machine for
	 * at least one whole `V17TX_modem` call's budget under the arm this
	 * run drives, so those are checked -- BRIDGE (6) only under long
	 * training, EQCOND's own arm always.
	 */
	rc = diff_end();

	{
		static const int must[] = { 1, 2, 3, 4, 5, 8 };

		diff_begin("v17txcreate: state coverage");
		for (i = 0; i < (int)(sizeof(must) / sizeof(must[0])); i++)
			diff_eq_int("state %ld visited during the run",
				    seen_state[must[i]], 1, must[i]);
		if (!short_train)
			diff_eq_int("state V17TX_STATE_BRIDGE visited (long train) (%ld)",
				    seen_state[V17TX_STATE_BRIDGE], 1, 0);
		rc |= diff_end();
	}

	V17TX_delete(a);
	ref_V17TX_delete(b);

	return rc;
}

static int
test_tx_cycle_long_train(void)
{
	return run_tx_cycle("v17txcreate: the half-duplex machine, long training",
			     0, 171717u, 900);
}

static int
test_tx_cycle_short_train(void)
{
	return run_tx_cycle("v17txcreate: the half-duplex machine, short training",
			     1, 292929u, 200);
}

/*
 * The one arm `V17TX_modem` alone cannot reliably reach: `TxHdxDataV17`'s
 * `V17TXP_INT_0008 != 0` underrun arm, which leaves the remainder in
 * `*budget` and calls `TxNextStateV17` rather than consuming the whole
 * request.  Reached by calling `TxHdxDataV17` directly against a handle
 * `V17TX_create` built, with the FIFO underfed and the field poked by
 * offset.
 */
static int
test_data_underrun_bypass_arm(void)
{
	void *a, *b;
	struct v17tx_cfg ca, cb;
	void *pa, *pb;
	struct fax_fifo *fa, *fb;
	unsigned short ina[8], inb[8], qa[2], qb[2];
	short outa[512], outb[512];
	short budgeta, budgetb;
	short reta, retb;

	diff_begin("v17txcreate: TxHdxDataV17's V17TXP_INT_0008 != 0 underrun arm");

	ca = V17TX_CFG;
	cb = V17TX_CFG;
	a = V17TX_create(0, &ca);
	b = ref_V17TX_create(0, &cb);
	diff_eq_int("both built (%ld)", a != 0 && b != 0, 1, 0);
	if (a == 0 || b == 0)
		return diff_end();

	pa = FIELD_PTR(a, V17TX_OBJ_PARAMS);
	pb = FIELD_PTR(b, V17TX_OBJ_PARAMS);
	fa = (struct fax_fifo *)FIELD_PTR(pa, V17TXP_FIFO);
	fb = (struct fax_fifo *)FIELD_PTR(pb, V17TXP_FIFO);

	/* Queue 2 elements, ask TxHdxDataV17 for 8: an underrun. */
	qa[0] = qa[1] = 1;
	qb[0] = qb[1] = 1;
	FIFO_write(fa, qa, 2);
	FIFO_write(fb, qb, 2);

	AT_I(pa, V17TXP_INT_0008) = 1;
	AT_I(pb, V17TXP_INT_0008) = 1;
	AT_S(pa, V17TXP_STATE) = V17TX_STATE_DATA;
	AT_S(pb, V17TXP_STATE) = V17TX_STATE_DATA;
	AT_S(pa, V17TXP_SHORT_001A) = 0;
	AT_S(pb, V17TXP_SHORT_001A) = 0;

	/*
	 * `ModDataV17` indexes `V17FP_ENCODERS` by `V17FP_ENCODER_SEL`, and
	 * `V17TX_create` never writes that field -- only `SetEncoderV17`/
	 * `SetTxModeV17` do, both called from `TxNextStateV17`'s own arms on
	 * the way to `DATA` in a natural cycle (BRIDGE/EQCOND's
	 * `SetEncoderV17(modem, V17_ENCODER_TCM, ...)`).  Calling
	 * `TxHdxDataV17` directly, bypassing that path, leaves the field as
	 * whatever `sysdep_malloc` handed back and `ModDataV17` calls
	 * through it -- primed here the same way the real cycle would have
	 * left it.
	 */
	SetEncoderV17(a, V17_ENCODER_TCM, 0);
	SetEncoderV17(b, V17_ENCODER_TCM, 0);

	memset(ina, 0xa5, sizeof ina);
	memset(inb, 0xa5, sizeof inb);
	memset(outa, 0xa5, sizeof outa);
	memset(outb, 0xa5, sizeof outb);
	budgeta = budgetb = 8;

	reta = TxHdxDataV17(a, ina, outa, &budgeta);
	retb = ref_TxHdxDataV17(b, inb, outb, &budgetb);

	diff_eq_int("return (%ld)", reta, retb, 0);
	diff_eq_int("*budget LEFT NON-ZERO (%ld)", budgeta, budgetb, 0);
	diff_eq_int("*budget is 6 (8 requested - 2 taken) (%ld)", budgeta,
		    6, 0);
	/*
	 * `out[]` is NOT compared here: `SetEncoderV17(..., V17_ENCODER_TCM,
	 * ...)` primes `V17FP_ENCODER_SEL` (see the comment above) but not
	 * `SMCv17_encoder_tcm`'s own trellis state (`SMC_TRELLIS`/
	 * `SMC_PREV`), which a real cycle would have carried forward from
	 * `TxHdxBridgeV17`/`TxHdxEQCondV17`'s own earlier symbols and which
	 * this bypass has none of -- so the sample VALUES are whatever the
	 * two sides' independent allocators happened to leave there, the
	 * same "untestable either way" ground D1291/D1331 stand on for the
	 * SGD generator's own unset fields. The return value, the budget
	 * arithmetic and the state machine's own bookkeeping (checked below)
	 * are what this arm exists to prove.
	 */
	diff_eq_int("params.state advanced, ours (%ld)",
		    fn_id(FIELD_PTR(pa, V17TXP_PROCESS)), 6, 0);
	diff_eq_int("params.state advanced, blob's (%ld)",
		    fn_id(FIELD_PTR(pb, V17TXP_PROCESS)), 6, 0);
	diff_eq_int("params.state is V17TX_STATE_SCR1_END, ours (%ld)",
		    AT_S(pa, V17TXP_STATE), V17TX_STATE_SCR1_END, 0);

	/*
	 * SCR1_END -> QUIET_END -> IDLE -> START, by direct calls -- the
	 * rest of the chain `test_tx_cycle_*` cannot reach either, per
	 * `run_tx_cycle`'s own comment on why QUIET_END/IDLE are excluded
	 * from its coverage requirement.  Per `v17fax.h`'s own state table:
	 * SCR1_END installs `TxHdxSilenceV17` (fn_id 1) and, like `DATA`,
	 * clears B2's bit and SETS B1's; QUIET_END installs `TxHdxIdleV17`
	 * (fn_id 8), sets `V17TXP_INT_0008` and SETS B2's bit; IDLE installs
	 * `TxHdxStartV17` (fn_id 0) through the machine's ordinary shared
	 * tail.
	 */
	TxNextStateV17(a);
	ref_TxNextStateV17(b);
	diff_eq_int("SCR1_END -> QUIET_END, ours (%ld)",
		    AT_S(pa, V17TXP_STATE), V17TX_STATE_QUIET_END, 1);
	diff_eq_int("SCR1_END -> QUIET_END, blob's (%ld)",
		    AT_S(pb, V17TXP_STATE), V17TX_STATE_QUIET_END, 1);
	diff_eq_int("process, ours vs blob's (%ld)",
		    fn_id(FIELD_PTR(pa, V17TXP_PROCESS)),
		    fn_id(FIELD_PTR(pb, V17TXP_PROCESS)), 1);
	diff_eq_int("process is TxHdxSilenceV17 (%ld)",
		    fn_id(FIELD_PTR(pa, V17TXP_PROCESS)), 1, 1);
	diff_eq_int("RESULT_B2_BIT0 cleared, ours (%ld)",
		    (*FIELD(a, V17TX_OBJ_RESULT_B2)
		     & V17TX_RESULT_B2_BIT0) != 0, 0, 1);
	diff_eq_int("RESULT_B1_BIT0 set, ours (%ld)",
		    (*FIELD(a, V17TX_OBJ_RESULT_B1)
		     & V17TX_RESULT_B1_BIT0) != 0, 1, 1);

	TxNextStateV17(a);
	ref_TxNextStateV17(b);
	diff_eq_int("QUIET_END -> IDLE, ours (%ld)",
		    AT_S(pa, V17TXP_STATE), V17TX_STATE_IDLE, 2);
	diff_eq_int("QUIET_END -> IDLE, blob's (%ld)",
		    AT_S(pb, V17TXP_STATE), V17TX_STATE_IDLE, 2);
	diff_eq_int("process, ours vs blob's (%ld)",
		    fn_id(FIELD_PTR(pa, V17TXP_PROCESS)),
		    fn_id(FIELD_PTR(pb, V17TXP_PROCESS)), 2);
	diff_eq_int("process is TxHdxIdleV17 (%ld)",
		    fn_id(FIELD_PTR(pa, V17TXP_PROCESS)), 8, 2);
	diff_eq_int("int_0008 (%ld)", AT_I(pa, V17TXP_INT_0008),
		    AT_I(pb, V17TXP_INT_0008), 2);
	diff_eq_int("int_0008 is 1 (%ld)", AT_I(pa, V17TXP_INT_0008), 1, 2);
	diff_eq_int("RESULT_B2_BIT0 set, ours (%ld)",
		    (*FIELD(a, V17TX_OBJ_RESULT_B2)
		     & V17TX_RESULT_B2_BIT0) != 0, 1, 2);

	TxNextStateV17(a);
	ref_TxNextStateV17(b);
	diff_eq_int("IDLE -> START, ours (%ld)",
		    AT_S(pa, V17TXP_STATE), V17TX_STATE_START, 3);
	diff_eq_int("IDLE -> START, blob's (%ld)",
		    AT_S(pb, V17TXP_STATE), V17TX_STATE_START, 3);
	diff_eq_int("process, ours vs blob's (%ld)",
		    fn_id(FIELD_PTR(pa, V17TXP_PROCESS)),
		    fn_id(FIELD_PTR(pb, V17TXP_PROCESS)), 3);
	diff_eq_int("process is TxHdxStartV17 (%ld)",
		    fn_id(FIELD_PTR(pa, V17TXP_PROCESS)), 0, 3);

	V17TX_delete(a);
	ref_V17TX_delete(b);

	return diff_end();
}

/*
 * `TxNextStateV17`'s out-of-range default arm, `t_v29txcreate.c`'s idiom.
 */
static int
test_next_state_default_arm(void)
{
	void *a, *b;
	struct v17tx_cfg ca, cb;
	void *pa, *pb;
	short states[3] = { 12, -5, 100 };
	int i;

	diff_begin("v17txcreate: TxNextStateV17's out-of-range default arm");

	ca = V17TX_CFG;
	cb = V17TX_CFG;
	a = V17TX_create(0, &ca);
	b = ref_V17TX_create(0, &cb);
	diff_eq_int("both built (%ld)", a != 0 && b != 0, 1, 0);
	if (a == 0 || b == 0)
		return diff_end();

	pa = FIELD_PTR(a, V17TX_OBJ_PARAMS);
	pb = FIELD_PTR(b, V17TX_OBJ_PARAMS);

	for (i = 0; i < 3; i++) {
		AT_S(pa, V17TXP_STATE) = states[i];
		AT_S(pb, V17TXP_STATE) = states[i];
		*FIELD(a, V17TX_OBJ_RESULT_B1) = 0;
		*FIELD(b, V17TX_OBJ_RESULT_B1) = 0;
		*FIELD(a, V17TX_OBJ_RESULT_B2) = 0xff;
		*FIELD(b, V17TX_OBJ_RESULT_B2) = 0xff;

		TxNextStateV17(a);
		ref_TxNextStateV17(b);

		diff_eq_int("state unchanged, ours (%ld)",
			    AT_S(pa, V17TXP_STATE), states[i], i);
		diff_eq_int("state unchanged, blob's (%ld)",
			    AT_S(pb, V17TXP_STATE), states[i], i);
		diff_eq_int("status is V17TX_RESULT_BYTE_07 (%ld)",
			    *FIELD(a, V17TX_OBJ_RESULT), V17TX_RESULT_BYTE_07,
			    i);
		diff_eq_int("RESULT_B1 (%ld)", *FIELD(a, V17TX_OBJ_RESULT_B1),
			    *FIELD(b, V17TX_OBJ_RESULT_B1), i);
		diff_eq_int("RESULT_B1_BIT1 set (%ld)",
			    (*FIELD(a, V17TX_OBJ_RESULT_B1)
			     & V17TX_RESULT_B1_BIT1) != 0, 1, i);
		diff_eq_int("RESULT_B2 (%ld)", *FIELD(a, V17TX_OBJ_RESULT_B2),
			    *FIELD(b, V17TX_OBJ_RESULT_B2), i);
		diff_eq_int("RESULT_B2_BIT0 cleared (%ld)",
			    (*FIELD(a, V17TX_OBJ_RESULT_B2)
			     & V17TX_RESULT_B2_BIT0) != 0, 0, i);
	}

	V17TX_delete(a);
	ref_V17TX_delete(b);

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
	rc |= test_tx_cycle_long_train();
	rc |= test_tx_cycle_short_train();
	rc |= test_data_underrun_bypass_arm();
	rc |= test_next_state_default_arm();

	return rc;
}

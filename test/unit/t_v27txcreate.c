/*
 * t_v27txcreate.c -- differential test of `V27TX_create` (.text 0x09a330,
 *                    1,165 bytes) and the transmit half-duplex machine it
 *                    installs: `TxNextStateV27`, the seven `TxHdx*V27`
 *                    states, `TxNoCarrierV27` and `SetScramblerV27`.
 *
 * `t_v29txcreate.c`'s own four-layer shape, one modulation over:
 *
 *   1. WIRING.  `V27TX_create` self-allocating over both bit rates and the
 *      default-arm case, comparing the handle's config bytes, the
 *      data-source block and the private TX block's shallow fields against
 *      the blob.
 *
 *   2. RE-INITIALISATION IN PLACE, D955/F8587 planted.
 *
 *   3. THE HALF-DUPLEX MACHINE THROUGH REPEATED `TxNextStateV27`/`TxHdx*V27`
 *      CALLS, over many consecutive blocks with the FIFO sometimes fed and
 *      sometimes left to run dry -- what drives the machine around its
 *      eleven-state cycle far enough that every `TxHdx*V27` installs on
 *      both sides at least once.  There is no reconstructed `V27TX_modem`
 *      dispatcher (it was not in `V27TX_create`'s own closure), so this
 *      layer drives the installed handler directly rather than through one.
 *
 *   4. THE ARMS LAYER 3 CANNOT REACH RELIABLY: `TxHdxDataV27`'s
 *      underrun-with-`V27TXP_INT_0008`-set arm (nothing reconstructed ever
 *      sets that field non-zero) and `TxNextStateV27`'s out-of-range default
 *      arm.  Both reached by calling the half-duplex functions directly
 *      against a handle `V27TX_create` built, `t_v29txcreate.c`'s own idiom.
 */

#include <stddef.h>
#include <stdio.h>
#include <string.h>

#include "harness.h"

#include "dsplib/faxfifo.h"
#include "dsplib/fpm_pps.h"
#include "dsplib/fpm_smc.h"
#include "dsplib/sdmv27.h"
#include "dsplib/sgd.h"
#include "dsplib/smc.h"
#include "dsplib/sysdep.h"
#include "dsplib/v27cfg.h"
#include "dsplib/v27fax.h"

extern void *ref_V27TX_create(void *modem, const struct v27tx_cfg *params);
extern void ref_V27TX_delete(void *modem);
extern short ref_TxHdxStartV27(void *modem, unsigned short *in, short *out,
			       short *budget);
extern short ref_TxHdxQuietV27(void *modem, unsigned short *in, short *out,
			       short *budget);
extern short ref_TxHdxAltV27(void *modem, unsigned short *in, short *out,
			     short *budget);
extern short ref_TxHdxEQCondV27(void *modem, unsigned short *in, short *out,
				short *budget);
extern short ref_TxHdxSCR1V27(void *modem, unsigned short *in, short *out,
			      short *budget);
extern short ref_TxHdxDataV27(void *modem, unsigned short *in, short *out,
			      short *budget);
extern short ref_TxHdxIdleV27(void *modem, unsigned short *in, short *out,
			      short *budget);
extern short ref_TxNoCarrierV27(void *modem, unsigned short *in, short *out,
				unsigned short count);
extern int ref_V27TX_modem(void *modem, unsigned short *in, short *out,
			   unsigned short *count);
extern void ref_TxNextStateV27(void *modem);
extern void ref_SetScramblerV27(void *modem);
extern void ref_GenEQTrnSequenceV27(void *modem, unsigned short *buf,
				    unsigned short count);
extern struct v27tx_cfg ref_V27TX_CFG;

#define FIELD(obj, off)		((unsigned char *)(void *)(obj) + (off))
#define FIELD_PTR(obj, off)	(*(void **)(void *)FIELD((obj), (off)))
#define AT_S(obj, off)		(*(short *)(void *)FIELD((obj), (off)))
#define AT_I(obj, off)		(*(int *)(void *)FIELD((obj), (off)))
#define AT_B(obj, off)		(*(unsigned char *)FIELD((obj), (off)))

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

/* Which of the seven transmit handlers a pointer is, on EITHER side. */
static int
fn_id(const void *p)
{
	if (p == (const void *)TxHdxStartV27 || p == (const void *)&ref_TxHdxStartV27)
		return 0;
	if (p == (const void *)TxHdxQuietV27 || p == (const void *)&ref_TxHdxQuietV27)
		return 1;
	if (p == (const void *)TxHdxAltV27 || p == (const void *)&ref_TxHdxAltV27)
		return 2;
	if (p == (const void *)TxHdxEQCondV27 || p == (const void *)&ref_TxHdxEQCondV27)
		return 3;
	if (p == (const void *)TxHdxSCR1V27 || p == (const void *)&ref_TxHdxSCR1V27)
		return 4;
	if (p == (const void *)TxHdxDataV27 || p == (const void *)&ref_TxHdxDataV27)
		return 5;
	if (p == (const void *)TxHdxIdleV27 || p == (const void *)&ref_TxHdxIdleV27)
		return 6;
	return -1;
}

/* ------------------------------------------------------------------------- */

static int
test_shape(void)
{
	diff_begin("v27txcreate: sizes against the object's symbol table");

	diff_eq_int("sizeof(struct v27tx_cfg) (%ld)",
		    (long)sizeof(struct v27tx_cfg), 0x20, 0);
	diff_eq_int("sizeof V27TX_CFG (%ld)", (long)sizeof(V27TX_CFG), 0x20, 0);

	return diff_end();
}

/*
 * Everything `V27TX_create` builds, laid out to the private block's shallow
 * fields.
 */
static void
compare_tree(const char *what, void *a, void *b, long tag)
{
	void *pa = FIELD_PTR(a, V27_OBJ_TXDATA);
	void *pb = FIELD_PTR(b, V27_OBJ_TXDATA);
	void *ta = FIELD_PTR(a, V27_OBJ_TX);
	void *tb = FIELD_PTR(b, V27_OBJ_TX);
	struct fax_fifo *fa, *fb;
	struct sgd *sa, *sb;
	struct fpm_smc_ring *ra, *rb;
	struct fpm_smc *sma, *smb;
	struct fpm_pps *ppa, *ppb;
	struct sdmv27 *sda, *sdb;
	char buf[160];

	diff_eq_int("data block allocated on both sides (%ld)",
		    pa != 0 && pb != 0, 1, tag);
	diff_eq_int("tx block allocated on both sides (%ld)",
		    ta != 0 && tb != 0, 1, tag);
	if (pa == 0 || pb == 0 || ta == 0 || tb == 0)
		return;

	/* The config, the handle's first 32 bytes. */
	snprintf(buf, sizeof(buf), "%.90s config bytes (%%ld)", what);
	diff_eq_int(buf, memcmp(a, b, sizeof(struct v27tx_cfg)) == 0, 1, tag);

	/* The result word: status, and the two flag bytes. */
	snprintf(buf, sizeof(buf), "%.90s result status (%%ld)", what);
	diff_eq_int(buf, AT_B(a, V27TX_OBJ_RESULT), AT_B(b, V27TX_OBJ_RESULT),
		    tag);
	snprintf(buf, sizeof(buf), "%.90s result B1 (%%ld)", what);
	diff_eq_int(buf, AT_B(a, V27TX_OBJ_RESULT_B1),
		    AT_B(b, V27TX_OBJ_RESULT_B1), tag);
	snprintf(buf, sizeof(buf), "%.90s result B2 (%%ld)", what);
	diff_eq_int(buf, AT_B(a, V27TX_OBJ_RESULT_B2),
		    AT_B(b, V27TX_OBJ_RESULT_B2), tag);

	/* The data-source / half-duplex block. */
	snprintf(buf, sizeof(buf), "%.90s params.state (%%ld)", what);
	diff_eq_int(buf, AT_S(pa, V27TXP_STATE), AT_S(pb, V27TXP_STATE), tag);
	snprintf(buf, sizeof(buf), "%.90s params.countdown (%%ld)", what);
	diff_eq_int(buf, AT_S(pa, V27TXP_COUNTDOWN), AT_S(pb, V27TXP_COUNTDOWN),
		    tag);
	snprintf(buf, sizeof(buf), "%.90s params.rate (%%ld)", what);
	diff_eq_int(buf, AT_S(pa, V27TXP_RATE), AT_S(pb, V27TXP_RATE), tag);
	snprintf(buf, sizeof(buf), "%.90s params.train_long (%%ld)", what);
	diff_eq_int(buf, AT_S(pa, V27TXP_TRAIN_LONG),
		    AT_S(pb, V27TXP_TRAIN_LONG), tag);
	snprintf(buf, sizeof(buf), "%.90s params.int_0008 (%%ld)", what);
	diff_eq_int(buf, AT_I(pa, V27TXP_INT_0008), AT_I(pb, V27TXP_INT_0008),
		    tag);
	snprintf(buf, sizeof(buf), "%.90s params.process, ours vs blob's (%%ld)",
		 what);
	diff_eq_int(buf, fn_id(FIELD_PTR(pa, V27TXP_PROCESS)),
		    fn_id(FIELD_PTR(pb, V27TXP_PROCESS)), tag);

	fa = (struct fax_fifo *)FIELD_PTR(pa, V27TXD_FIFO);
	fb = (struct fax_fifo *)FIELD_PTR(pb, V27TXD_FIFO);
	diff_eq_int("FIFO built on both sides (%ld)", fa != 0 && fb != 0,
		    1, tag);
	if (fa != 0 && fb != 0) {
		diff_eq_int("fifo.size (%ld)", fa->size, fb->size, tag);
		diff_eq_int("fifo.fill (%ld)", fa->fill, fb->fill, tag);
		diff_eq_int("fifo.count (%ld)", fa->count, fb->count, tag);
		diff_eq_int("fifo.rd (%ld)", fa->rd, fb->rd, tag);
		diff_eq_int("fifo.wr (%ld)", fa->wr, fb->wr, tag);
	}

	sa = (struct sgd *)FIELD_PTR(pa, V27TXD_SGD);
	sb = (struct sgd *)FIELD_PTR(pb, V27TXD_SGD);
	diff_eq_int("SGD built on both sides (%ld)", sa != 0 && sb != 0,
		    1, tag);
	if (sa != 0 && sb != 0) {
		diff_eq_int("sgd.cfg.sym_bits (%ld)", sa->cfg.sym_bits,
			    sb->cfg.sym_bits, tag);
		diff_eq_int("sgd.cfg.sym_bits is 3 (%ld)", sa->cfg.sym_bits,
			    3, tag);
		diff_eq_int("sgd.cfg.hist_len (%ld)", sa->cfg.hist_len,
			    sb->cfg.hist_len, tag);
	}

	/* The private block's ring: len, and the sym rail's contents. */
	ra = (struct fpm_smc_ring *)(void *)FIELD(ta, V27TX_RING);
	rb = (struct fpm_smc_ring *)(void *)FIELD(tb, V27TX_RING);
	diff_eq_int("tx.ring.len (%ld)", ra->len, rb->len, tag);
	diff_eq_int("tx.ring.widx (%ld)", ra->widx, rb->widx, tag);
	diff_eq_int("tx.ring.ridx (%ld)", ra->ridx, rb->ridx, tag);
	diff_eq_int("tx.ring.i is null, ours (%ld)", ra->i == 0, 1, tag);
	diff_eq_int("tx.ring.q is null, ours (%ld)", ra->q == 0, 1, tag);
	if (ra->len == rb->len && ra->len > 0 && ra->len <= 64)
		cmp_shorts("tx.ring.sym[%ld]", ra->sym, rb->sym, ra->len);

	/* The scrambler. */
	sda = (struct sdmv27 *)(void *)FIELD(ta, V27TX_SDM);
	sdb = (struct sdmv27 *)(void *)FIELD(tb, V27TX_SDM);
	diff_eq_int("tx.sdm.nbits (%ld)", sda->nbits, sdb->nbits, tag);
	diff_eq_int("tx.sdm.mask (%ld)", sda->mask, sdb->mask, tag);
	diff_eq_int("tx.sdm.notmask (%ld)", sda->notmask, sdb->notmask, tag);
	diff_eq_int("tx.sdm.reg (%ld)", sda->reg, sdb->reg, tag);

	/* The symbol coder: the scalar fields, not the table pointers. */
	sma = (struct fpm_smc *)(void *)FIELD(ta, V27TX_SMC);
	smb = (struct fpm_smc *)(void *)FIELD(tb, V27TX_SMC);
	diff_eq_int("tx.smc.cfg.f00 (%ld)", sma->cfg.f00, smb->cfg.f00, tag);
	diff_eq_int("tx.smc.cfg.direct (%ld)", sma->cfg.direct,
		    smb->cfg.direct, tag);
	diff_eq_int("tx.smc.cfg.rot_step (%ld)", sma->cfg.rot_step,
		    smb->cfg.rot_step, tag);
	diff_eq_int("tx.smc.cfg.rot_mod (%ld)", sma->cfg.rot_mod,
		    smb->cfg.rot_mod, tag);
	diff_eq_int("tx.smc.cfg.qshift (%ld)", sma->cfg.qshift,
		    smb->cfg.qshift, tag);
	diff_eq_int("tx.smc.cfg.qmask (%ld)", sma->cfg.qmask,
		    smb->cfg.qmask, tag);
	diff_eq_int("tx.smc.cfg.amask (%ld)", sma->cfg.amask,
		    smb->cfg.amask, tag);
	diff_eq_int("tx.smc.cfg.pmask (%ld)", sma->cfg.pmask,
		    smb->cfg.pmask, tag);
	diff_eq_int("tx.smc.quad (%ld)", sma->quad, smb->quad, tag);
	diff_eq_int("tx.smc.acc (%ld)", sma->acc, smb->acc, tag);

	/* The pulse shaper. */
	ppa = (struct fpm_pps *)(void *)FIELD(ta, V27TX_PPS);
	ppb = (struct fpm_pps *)(void *)FIELD(tb, V27TX_PPS);
	diff_eq_int("tx.pps.cfg.phases (%ld)", ppa->cfg.phases,
		    ppb->cfg.phases, tag);
	diff_eq_int("tx.pps.cfg.step (%ld)", ppa->cfg.step, ppb->cfg.step,
		    tag);
	diff_eq_int("tx.pps.cfg.mapped (%ld)", ppa->cfg.mapped,
		    ppb->cfg.mapped, tag);
	diff_eq_int("tx.pps.cfg.scale (%ld)", ppa->cfg.scale, ppb->cfg.scale,
		    tag);
	diff_eq_int("tx.pps.cfg.coeffs (%ld)", ppa->cfg.coeffs,
		    ppb->cfg.coeffs, tag);
	diff_eq_int("tx.pps.need (%ld)", ppa->need, ppb->need, tag);
	if (ppa->cfg.coeff_i != 0 && ppb->cfg.coeff_i != 0
	    && ppa->cfg.coeffs == ppb->cfg.coeffs && ppa->cfg.coeffs > 0
	    && ppa->cfg.coeffs <= 120)
		cmp_shorts("tx.pps.cfg.coeff_i[%ld]", ppa->cfg.coeff_i,
			   ppb->cfg.coeff_i, ppa->cfg.coeffs);
}

/* ------------------------------------------------------------------------- */

static const struct {
	const char *name;
	int use_default;
	short bitrate;
} cases[] = {
	{ "params NULL (V27TX_CFG)",                 1, 0 },
	{ "explicit, bitrate == 2400",                0, 2400 },
	{ "explicit, bitrate == 4800",                0, 4800 },
	{ "explicit, bitrate == 9600 (default arm)",  0, 9600 },
};
#define NCASES ((long)(sizeof(cases) / sizeof(cases[0])))

static void
build_cfg(struct v27tx_cfg *c, long k)
{
	*c = V27TX_CFG;
	c->bitrate = cases[k].bitrate;
}

static int
test_create_self_allocating(void)
{
	long k;

	diff_begin("v27txcreate: V27TX_create, self-allocating");

	for (k = 0; k < NCASES; k++) {
		struct v27tx_cfg ca, cb;
		void *a, *b;

		build_cfg(&ca, k);
		build_cfg(&cb, k);

		if (cases[k].use_default) {
			b = ref_V27TX_create(0, 0);
			a = V27TX_create(0, 0);
		} else {
			b = ref_V27TX_create(0, &cb);
			a = V27TX_create(0, &ca);
		}

		diff_eq_int("both built (%ld)", a != 0 && b != 0, 1, k);
		if (a == 0 || b == 0)
			continue;

		compare_tree(cases[k].name, a, b, k);

		{
			void *pa = FIELD_PTR(a, V27_OBJ_TXDATA);

			diff_eq_int("fresh: params.state is V27TX_STATE_START (%ld)",
				    AT_S(pa, V27TXP_STATE), V27TX_STATE_START,
				    k);
			diff_eq_int("fresh: params.process is TxHdxStartV27 (%ld)",
				    fn_id(FIELD_PTR(pa, V27TXP_PROCESS)), 0, k);
		}

		if (!cases[k].use_default && cases[k].bitrate != 2400
		    && cases[k].bitrate != 4800) {
			diff_eq_int("default-arm status is V27TX_STATUS_DEFAULT (%ld)",
				    AT_B(a, V27TX_OBJ_RESULT),
				    V27TX_STATUS_DEFAULT, k);
			diff_eq_int("default-arm sets RESULT_B1_BIT1 (%ld)",
				    (AT_B(a, V27TX_OBJ_RESULT_B1)
				     & V27TX_RESULT_B1_BIT1) != 0, 1, k);
			diff_eq_int("default-arm rate is 4800's index (%ld)",
				    AT_S(FIELD_PTR(a, V27_OBJ_TXDATA),
					 V27TXP_RATE),
				    1, k);
		}

		V27TX_delete(a);
		ref_V27TX_delete(b);
	}

	return diff_end();
}

/*
 * Re-initialisation in place, D955/F8587 planted: the whole handle and the
 * ring's sym buffer are overwritten with a non-zero pattern between two
 * calls (keeping only the sub-pointers the constructor tests to decide
 * whether to allocate), so a field the second call forgets to rewrite is
 * visibly the pattern rather than invisibly a zero the first call already
 * left.
 */
static int
test_reinit(void)
{
	void *a, *b;
	void *pa, *pb;
	void *txa, *txb;
	struct v27tx_cfg ca, cb;

	diff_begin("v27txcreate: V27TX_create, re-initialised in place");

	ca = V27TX_CFG;
	cb = V27TX_CFG;
	ca.bitrate = cb.bitrate = 4800;
	a = V27TX_create(0, &ca);
	b = ref_V27TX_create(0, &cb);
	diff_eq_int("both built (%ld)", a != 0 && b != 0, 1, 0);
	if (a == 0 || b == 0)
		return diff_end();

	pa = FIELD_PTR(a, V27_OBJ_TXDATA);
	pb = FIELD_PTR(b, V27_OBJ_TXDATA);
	txa = FIELD_PTR(a, V27_OBJ_TX);
	txb = FIELD_PTR(b, V27_OBJ_TX);

	memset(a, 0x5a, 0x2c);
	memset(b, 0x5a, 0x2c);
	FIELD_PTR(a, V27_OBJ_TXDATA) = pa;
	FIELD_PTR(b, V27_OBJ_TXDATA) = pb;
	FIELD_PTR(a, V27_OBJ_TX) = txa;
	FIELD_PTR(b, V27_OBJ_TX) = txb;

	a = V27TX_create(a, &ca);
	b = ref_V27TX_create(b, &cb);

	diff_eq_int("data block kept, ours (%ld)",
		    FIELD_PTR(a, V27_OBJ_TXDATA) == pa, 1, 0);
	diff_eq_int("data block kept, blob's (%ld)",
		    FIELD_PTR(b, V27_OBJ_TXDATA) == pb, 1, 0);
	diff_eq_int("tx block kept, ours (%ld)",
		    FIELD_PTR(a, V27_OBJ_TX) == txa, 1, 0);
	diff_eq_int("tx block kept, blob's (%ld)",
		    FIELD_PTR(b, V27_OBJ_TX) == txb, 1, 0);

	compare_tree("reinit", a, b, 0);

	diff_eq_int("reinit: params.state is V27TX_STATE_START (%ld)",
		    AT_S(pa, V27TXP_STATE), V27TX_STATE_START, 0);
	diff_eq_int("reinit: params.process is TxHdxStartV27 (%ld)",
		    fn_id(FIELD_PTR(pa, V27TXP_PROCESS)), 0, 0);

	V27TX_delete(a);
	ref_V27TX_delete(b);

	return diff_end();
}

/*
 * The half-duplex machine, driven by hand: call the currently-installed
 * handler with a random budget, over many consecutive blocks, with the FIFO
 * sometimes fed and sometimes left empty.  There is no reconstructed
 * `V27TX_modem` in `V27TX_create`'s own closure, so this plays that
 * dispatcher's role directly.
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
	struct v27tx_cfg ca, cb;
	unsigned seed = 272727u;
	int block;
	int seen_state[11];
	int i;

	diff_begin("v27txcreate: the half-duplex machine driven by hand");

	ca = V27TX_CFG;
	cb = V27TX_CFG;
	ca.bitrate = cb.bitrate = 4800;
	a = V27TX_create(0, &ca);
	b = ref_V27TX_create(0, &cb);
	diff_eq_int("both built (%ld)", a != 0 && b != 0, 1, 0);
	if (a == 0 || b == 0)
		return diff_end();

	for (i = 0; i < 11; i++)
		seen_state[i] = 0;

	for (block = 0; block < 600; block++) {
		unsigned short qbuf_a[64], qbuf_b[64];
		short outa[512], outb[512];
		int qn = (int)(rnd(&seed) % 9);
		short budgeta, budgetb;
		short reta, retb;
		void *pa, *pb;
		v27tx_process_fn pfa, pfb;
		short sta, stb;

		for (i = 0; i < qn; i++)
			qbuf_a[i] = qbuf_b[i] =
				(unsigned short)(rnd(&seed) & 1);

		pa = FIELD_PTR(a, V27_OBJ_TXDATA);
		pb = FIELD_PTR(b, V27_OBJ_TXDATA);

		if (qn > 0) {
			FIFO_write((struct fax_fifo *)
					FIELD_PTR(pa, V27TXD_FIFO),
				  qbuf_a, (unsigned short)qn);
			FIFO_write((struct fax_fifo *)
					FIELD_PTR(pb, V27TXD_FIFO),
				  qbuf_b, (unsigned short)qn);
		}

		budgeta = budgetb = (short)(4 + (rnd(&seed) % 40));
		memset(outa, 0xa5, sizeof outa);
		memset(outb, 0xa5, sizeof outb);
		memset(qbuf_a, 0xa5, sizeof qbuf_a);
		memset(qbuf_b, 0xa5, sizeof qbuf_b);

		pfa = *(v27tx_process_fn *)(void *)
			FIELD(pa, V27TXP_PROCESS);
		pfb = *(v27tx_process_fn *)(void *)
			FIELD(pb, V27TXP_PROCESS);

		diff_eq_int("installed handler agrees before block (%ld)",
			    fn_id((void *)pfa), fn_id((void *)pfb), block);

		reta = pfa(a, qbuf_a, outa, &budgeta);
		retb = pfb(b, qbuf_b, outb, &budgetb);

		diff_eq_int("handler return (%ld)", reta, retb, block);
		diff_eq_int("*budget (%ld)", budgeta, budgetb, block);
		if (reta == retb && reta > 0 && reta <= 512)
			cmp_shorts("out[%ld]", outa, outb, reta);

		pa = FIELD_PTR(a, V27_OBJ_TXDATA);
		pb = FIELD_PTR(b, V27_OBJ_TXDATA);
		sta = AT_S(pa, V27TXP_STATE);
		stb = AT_S(pb, V27TXP_STATE);
		diff_eq_int("params.state agrees (%ld)", sta, stb, block);
		if (sta >= 0 && sta < 11)
			seen_state[sta] = 1;
	}

	/*
	 * States 0 (START), 8 (TURNOFF), 9 (NOENG) and 10 (IDLE) are reached
	 * only via `TxHdxDataV27`'s underrun-bypass arm -- nothing in ordinary
	 * operation ever transitions the machine OUT of DATA once it arrives
	 * there, so a driven cycle that never pokes `V27TXP_INT_0008` cannot
	 * reach them.  `test_data_underrun_bypass_arm` below reaches the one
	 * transition (DATA -> TURNOFF) that path exercises; the rest of that
	 * chain is untested, same as V29's own unreachable-without-poking
	 * corners.  Only 1..7 (QUIET..DATA) are asserted here.
	 */
	for (i = 1; i <= 7; i++)
		diff_eq_int("state %ld visited during the run", seen_state[i],
			    1, i);

	compare_tree("after driven cycle", a, b, 0);

	V27TX_delete(a);
	ref_V27TX_delete(b);

	return diff_end();
}

/*
 * The one arm the driven cycle above cannot reliably reach:
 * `TxHdxDataV27`'s `V27TXP_INT_0008 != 0` underrun arm, which leaves the
 * remainder in `*budget`, calls `TxNextStateV27` and does NOT consume the
 * whole request.  Reached by calling `TxHdxDataV27` directly against a
 * handle `V27TX_create` built, with the FIFO underfed and the field poked by
 * offset.
 */
static int
test_data_underrun_bypass_arm(void)
{
	void *a, *b;
	struct v27tx_cfg ca, cb;
	void *pa, *pb;
	struct fax_fifo *fa, *fb;
	unsigned short ina[8], inb[8], qa[2], qb[2];
	short outa[512], outb[512];
	short budgeta, budgetb;
	short reta, retb;

	diff_begin("v27txcreate: TxHdxDataV27's V27TXP_INT_0008 != 0 underrun arm");

	ca = V27TX_CFG;
	cb = V27TX_CFG;
	ca.bitrate = cb.bitrate = 4800;
	a = V27TX_create(0, &ca);
	b = ref_V27TX_create(0, &cb);
	diff_eq_int("both built (%ld)", a != 0 && b != 0, 1, 0);
	if (a == 0 || b == 0)
		return diff_end();

	pa = FIELD_PTR(a, V27_OBJ_TXDATA);
	pb = FIELD_PTR(b, V27_OBJ_TXDATA);
	fa = (struct fax_fifo *)FIELD_PTR(pa, V27TXD_FIFO);
	fb = (struct fax_fifo *)FIELD_PTR(pb, V27TXD_FIFO);

	/* Queue 2 elements, ask TxHdxDataV27 for 8: an underrun. */
	qa[0] = qa[1] = 1;
	qb[0] = qb[1] = 1;
	FIFO_write(fa, qa, 2);
	FIFO_write(fb, qb, 2);

	AT_I(pa, V27TXP_INT_0008) = 1;
	AT_I(pb, V27TXP_INT_0008) = 1;
	AT_S(pa, V27TXP_STATE) = V27TX_STATE_DATA;
	AT_S(pb, V27TXP_STATE) = V27TX_STATE_DATA;
	AT_S(pa, V27TXP_COUNTDOWN) = 0;
	AT_S(pb, V27TXP_COUNTDOWN) = 0;

	memset(ina, 0xa5, sizeof ina);
	memset(inb, 0xa5, sizeof inb);
	memset(outa, 0xa5, sizeof outa);
	memset(outb, 0xa5, sizeof outb);
	budgeta = budgetb = 8;

	reta = TxHdxDataV27(a, ina, outa, &budgeta);
	retb = ref_TxHdxDataV27(b, inb, outb, &budgetb);

	diff_eq_int("return (%ld)", reta, retb, 0);
	diff_eq_int("*budget LEFT NON-ZERO (%ld)", budgeta, budgetb, 0);
	diff_eq_int("*budget is 6 (8 requested - 2 taken) (%ld)", budgeta,
		    6, 0);
	cmp_shorts("out[%ld]", outa, outb, reta < retb ? reta : retb);
	diff_eq_int("params.process advanced to TxHdxSCR1V27, ours (%ld)",
		    fn_id(FIELD_PTR(pa, V27TXP_PROCESS)), 4, 0);
	diff_eq_int("params.process advanced to TxHdxSCR1V27, blob's (%ld)",
		    fn_id(FIELD_PTR(pb, V27TXP_PROCESS)), 4, 0);
	diff_eq_int("params.state is V27TX_STATE_TURNOFF, ours (%ld)",
		    AT_S(pa, V27TXP_STATE), V27TX_STATE_TURNOFF, 0);
	diff_eq_int("params.state is V27TX_STATE_TURNOFF, blob's (%ld)",
		    AT_S(pb, V27TXP_STATE), V27TX_STATE_TURNOFF, 0);

	V27TX_delete(a);
	ref_V27TX_delete(b);

	return diff_end();
}

/*
 * `TxNextStateV27`'s out-of-range default arm, `t_v29txcreate.c`'s idiom.
 */
static int
test_next_state_default_arm(void)
{
	void *a, *b;
	struct v27tx_cfg ca, cb;
	void *pa, *pb;
	short states[3] = { 11, -5, 100 };
	int i;

	diff_begin("v27txcreate: TxNextStateV27's out-of-range default arm");

	ca = V27TX_CFG;
	cb = V27TX_CFG;
	a = V27TX_create(0, &ca);
	b = ref_V27TX_create(0, &cb);
	diff_eq_int("both built (%ld)", a != 0 && b != 0, 1, 0);
	if (a == 0 || b == 0)
		return diff_end();

	pa = FIELD_PTR(a, V27_OBJ_TXDATA);
	pb = FIELD_PTR(b, V27_OBJ_TXDATA);

	for (i = 0; i < 3; i++) {
		AT_S(pa, V27TXP_STATE) = states[i];
		AT_S(pb, V27TXP_STATE) = states[i];
		AT_B(a, V27TX_OBJ_RESULT_B1) = 0;
		AT_B(b, V27TX_OBJ_RESULT_B1) = 0;
		AT_B(a, V27TX_OBJ_RESULT_B2) = 0xff;
		AT_B(b, V27TX_OBJ_RESULT_B2) = 0xff;

		TxNextStateV27(a);
		ref_TxNextStateV27(b);

		diff_eq_int("state unchanged, ours (%ld)",
			    AT_S(pa, V27TXP_STATE), states[i], i);
		diff_eq_int("state unchanged, blob's (%ld)",
			    AT_S(pb, V27TXP_STATE), states[i], i);
		diff_eq_int("status is V27TX_STATUS_DEFAULT (%ld)",
			    AT_B(a, V27TX_OBJ_RESULT), V27TX_STATUS_DEFAULT,
			    i);
		diff_eq_int("RESULT_B1 (%ld)", AT_B(a, V27TX_OBJ_RESULT_B1),
			    AT_B(b, V27TX_OBJ_RESULT_B1), i);
		diff_eq_int("RESULT_B1_BIT1 set (%ld)",
			    (AT_B(a, V27TX_OBJ_RESULT_B1)
			     & V27TX_RESULT_B1_BIT1) != 0, 1, i);
		diff_eq_int("RESULT_B2 (%ld)", AT_B(a, V27TX_OBJ_RESULT_B2),
			    AT_B(b, V27TX_OBJ_RESULT_B2), i);
		diff_eq_int("RESULT_B2_BIT0 cleared (%ld)",
			    (AT_B(a, V27TX_OBJ_RESULT_B2)
			     & V27TX_RESULT_B2_BIT0) != 0, 0, i);
	}

	V27TX_delete(a);
	ref_V27TX_delete(b);

	return diff_end();
}

/*
 * `V27TX_modem` itself, the real entry point -- `test_tx_cycle`'s own
 * driving loop, but through the dispatcher rather than calling the
 * installed handler by hand.  `in` is caller-owned scratch for the whole
 * call (never advanced), and `*count` is IN/OUT: samples queued on the way
 * in, samples produced on the way out.
 */
static int
test_modem_cycle(void)
{
	void *a, *b;
	struct v27tx_cfg ca, cb;
	unsigned seed = 292919u;
	int block;
	int seen_state[11];
	int i;

	diff_begin("v27txcreate: V27TX_modem, the real entry point");

	ca = V27TX_CFG;
	cb = V27TX_CFG;
	ca.bitrate = cb.bitrate = 2400;
	a = V27TX_create(0, &ca);
	b = ref_V27TX_create(0, &cb);
	diff_eq_int("both built (%ld)", a != 0 && b != 0, 1, 0);
	if (a == 0 || b == 0)
		return diff_end();

	for (i = 0; i < 11; i++)
		seen_state[i] = 0;

	for (block = 0; block < 800; block++) {
		/*
		 * `in` is scratch for the WHOLE `V27TXP_PROCESS` dispatch
		 * loop, not just the initial `FIFO_write` -- QUIET's own
		 * budget can reach `V27TX_FRMSIZE[rate] * 10` (up to 320 at
		 * 4800 bit/s).  512 is headroom, not the queued count.
		 */
		unsigned short qbuf_a[512], qbuf_b[512];
		short outa[512], outb[512];
		unsigned short counta, countb;
		int reta, retb;
		void *pa, *pb;
		short sta, stb;
		int qn = (int)(rnd(&seed) % 9);

		for (i = 0; i < qn; i++)
			qbuf_a[i] = qbuf_b[i] =
				(unsigned short)(rnd(&seed) & 1);

		counta = countb = (unsigned short)qn;
		memset(outa, 0xa5, sizeof outa);
		memset(outb, 0xa5, sizeof outb);

		reta = V27TX_modem(a, qbuf_a, outa, &counta);
		retb = ref_V27TX_modem(b, qbuf_b, outb, &countb);

		diff_eq_int("V27TX_modem return (%ld)", reta, retb, block);
		diff_eq_int("V27TX_modem *count (%ld)", counta, countb, block);
		if (counta == countb && counta <= 512)
			cmp_shorts("out[%ld]", outa, outb, counta);

		pa = FIELD_PTR(a, V27_OBJ_TXDATA);
		pb = FIELD_PTR(b, V27_OBJ_TXDATA);
		sta = AT_S(pa, V27TXP_STATE);
		stb = AT_S(pb, V27TXP_STATE);
		diff_eq_int("params.state agrees (%ld)", sta, stb, block);
		if (sta >= 0 && sta < 11)
			seen_state[sta] = 1;
	}

	for (i = 1; i <= 7; i++)
		diff_eq_int("state %ld visited during the run", seen_state[i],
			    1, i);

	compare_tree("after V27TX_modem cycle", a, b, 0);

	V27TX_delete(a);
	ref_V27TX_delete(b);

	return diff_end();
}

/*
 * `GenEQTrnSequenceV27` -- the free-standing generator, over the two rates
 * and a handful of counts (0 included, since the object's own loop guard
 * treats it specially).
 */
static int
test_gen_eq_trn_sequence(void)
{
	static const unsigned short counts[] = { 0, 1, 2, 7, 8, 63, 64 };
	long ci;
	int rate;

	diff_begin("v27txcreate: GenEQTrnSequenceV27");

	for (rate = 0; rate < 2; rate++) {
		for (ci = 0; ci < (long)(sizeof(counts) / sizeof(counts[0]));
		     ci++) {
			void *a, *b;
			struct v27tx_cfg ca, cb;
			unsigned short bufa[66], bufb[66];
			unsigned short count = counts[ci];
			long tag = rate * 100 + ci;

			ca = V27TX_CFG;
			cb = V27TX_CFG;
			ca.bitrate = cb.bitrate = (short)(rate == 0 ? 2400 : 4800);
			a = V27TX_create(0, &ca);
			b = ref_V27TX_create(0, &cb);
			diff_eq_int("both built (%ld)", a != 0 && b != 0, 1,
				    tag);
			if (a == 0 || b == 0)
				continue;

			memset(bufa, 0xa5, sizeof bufa);
			memset(bufb, 0xa5, sizeof bufb);

			GenEQTrnSequenceV27(a, bufa, count);
			ref_GenEQTrnSequenceV27(b, bufb, count);

			if (count > 0 && count <= 64)
				cmp_shorts("buf[%ld]", (short *)bufa,
					   (short *)bufb, (int)count);

			V27TX_delete(a);
			ref_V27TX_delete(b);
		}
	}

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
	rc |= test_modem_cycle();
	rc |= test_gen_eq_trn_sequence();

	return rc;
}

/*
 * t_class1delete.c -- differential test of `fax_class1_delete` (class1.c,
 * `.text` 0x0093bf0, 347 bytes) and, alongside it, of the two silence state
 * handlers `_t30_silence_before_tx_state` and `_tx_silence_before_scrm_ones`
 * (class1tx.c) that `_put_silence` unblocked this wave.
 *
 * `fax_class1_delete` IS TESTED VIA THE ALLOCATION LOG, same technique as
 * `t_faxvmids.c` and `t_class1delmodem.c` -- but here the expected free
 * count is not hand-derived (eight fields, two of them shapes this batch
 * cannot fully type, made that arithmetic exactly the kind of thing that
 * went wrong once already this wave, finding F10054's `int_0014`). Instead
 * BOTH SIDES build an IDENTICALLY SHAPED but SEPARATELY ALLOCATED session,
 * in the same `harness_alloc_reset()` window as the delete call, and the
 * test asserts `frees`/`live`/`bad_free` AGREE between ours and the blob's
 * -- which is what "freed the same graph" actually means -- rather than
 * asserting either side against a number derived by hand.
 */

#include <stddef.h>
#include <string.h>

#include "harness.h"
#include "dsplib/class1.h"
#include "dsplib/class1rx.h"
#include "dsplib/class1tx.h"
#include "dsplib/debug.h"
#include "dsplib/faxcfg.h"
#include "dsplib/faxfifo.h"
#include "dsplib/faxvmi.h"
#include "dsplib/fpm_tone.h"
#include "dsplib/sysdep.h"

extern unsigned int ref_dsplibs_debug_level;

extern int ref_fax_class1_delete(struct fax_class1 *ctx);

extern void ref_init_vmi_v27rx(struct faxvmi_cfg *vmi, unsigned short bit_rate,
			       int arg_2, void *arg_3);
extern int ref_init_vmi_v27tx(struct faxvmi_cfg *vmi, unsigned short bit_rate,
			      int arg_2, void *arg_3);

/*
 * FILE-LOCAL in the object, so class1rx.c/class1tx.c define them `static`
 * and the headers no longer declare them.  Their addresses are taken (stored
 * in the RX/TX dispatch tables), so the ordinary calling convention is
 * unchanged; the test tier links a globalized copy (tools/testvisible.py).
 */
extern void init_vmi_v27rx(struct faxvmi_cfg *vmi, unsigned short bit_rate,
			   int arg_2, void *arg_3);
extern int init_vmi_v27tx(struct faxvmi_cfg *vmi, unsigned short bit_rate,
			  int arg_2, void *arg_3);
extern struct fax_fifo *ref_FIFO_create(struct fax_fifo *f,
					const struct fifo_cfg *cfg);
extern struct fpm_tone *ref_FPM_TONE_create(struct fpm_tone *state,
					    const struct fpm_tone_cfg *cfg);

extern int ref__t30_silence_before_tx_state(struct fax_class1 *ctx,
	const short *rx, short *tx, int word3, int word4, int *rx_count,
	int *tx_count, int word7, int *word8);
extern int ref__tx_silence_before_scrm_ones(struct fax_class1 *ctx,
	const short *rx, short *tx, int word3, int word4, int *rx_count,
	int *tx_count, int word7, int *word8);

static int marker;

/* A NULL-slot `struct faxvmi`, same shape `t_faxvmids.c`/`t_class1delmodem.c`
 * already use. */
static struct faxvmi *
make_handle(void)
{
	struct faxvmi *vmi = sysdep_malloc(sizeof(*vmi));
	struct faxvmi_framer *fr = sysdep_malloc(sizeof(*fr));
	struct faxvmi_link *lk = sysdep_malloc(sizeof(*lk));

	fr->fifo = sysdep_malloc(16);
	fr->frame = sysdep_malloc(16);
	lk->ptr_0000 = sysdep_malloc(16);
	lk->buf = sysdep_malloc(16);
	vmi->slot = 0;
	vmi->framer = fr;
	vmi->link = lk;
	return vmi;
}

/* `vmi_c_cfg`/`vmi_a_cfg`'s shape: a pointer owning one sub-allocation at +0x10. */
static void *
make_sub14(void)
{
	unsigned char *p = sysdep_malloc(0x14);

	*(void **)(p + 0x10) = sysdep_malloc(4);
	return p;
}

/* rx_side: 1 builds an RX data modem (modem_direction = 1), 0 a TX one. */
static void
build_session(struct fax_class1 *ctx, int rx_side, int use_fpm)
{
	memset(ctx, 0, sizeof(*ctx));

	ctx->vmi_c_cfg = make_sub14();
	ctx->vmi_c = make_handle();
	ctx->vmi_a_cfg = make_sub14();
	ctx->vmi_a = make_handle();
	ctx->vmi_b = make_handle();

	ctx->modem_vmi = sysdep_malloc(sizeof(struct faxvmi_cfg));
	if (rx_side) {
		init_vmi_v27rx(ctx->modem_vmi, 4800, 0, &marker);
		ctx->modem_direction = 1;
	} else {
		init_vmi_v27tx(ctx->modem_vmi, 4800, 0, &marker);
		ctx->modem_direction = 0;
	}

	ctx->tx_fifo = FIFO_create(NULL, NULL);
	ctx->tone = use_fpm ? FPM_TONE_create(NULL, NULL) : NULL;
}

static void
build_session_ref(struct fax_class1 *ctx, int rx_side, int use_fpm)
{
	memset(ctx, 0, sizeof(*ctx));

	ctx->vmi_c_cfg = make_sub14();
	ctx->vmi_c = make_handle();
	ctx->vmi_a_cfg = make_sub14();
	ctx->vmi_a = make_handle();
	ctx->vmi_b = make_handle();

	ctx->modem_vmi = sysdep_malloc(sizeof(struct faxvmi_cfg));
	if (rx_side) {
		ref_init_vmi_v27rx(ctx->modem_vmi, 4800, 0, &marker);
		ctx->modem_direction = 1;
	} else {
		ref_init_vmi_v27tx(ctx->modem_vmi, 4800, 0, &marker);
		ctx->modem_direction = 0;
	}

	ctx->tx_fifo = ref_FIFO_create(NULL, NULL);
	ctx->tone = use_fpm ? ref_FPM_TONE_create(NULL, NULL) : NULL;
}

static void
run_case(int rx_side, int use_fpm, long input)
{
	struct fax_class1 *ca, *cb;
	long frees_a, live_a, bad_a, ret_a;
	long frees_b, live_b, bad_b, ret_b;

	/*
	 * `ctx` itself must be a heap object -- fax_class1_delete frees it
	 * -- and, per F10054's own lesson, it must be allocated INSIDE the
	 * same `harness_alloc_reset()` window as the delete call: allocating
	 * it first and resetting after would make the delete's own final
	 * free of `ctx` read as a bad free on a pointer the log never saw.
	 */
	harness_alloc_reset();
	ca = sysdep_malloc(sizeof(*ca));
	build_session(ca, rx_side, use_fpm);
	ret_a = fax_class1_delete(ca);
	frees_a = harness_alloc.frees;
	live_a = harness_alloc.live;
	bad_a = harness_alloc.bad_free;

	harness_alloc_reset();
	cb = sysdep_malloc(sizeof(*cb));
	build_session_ref(cb, rx_side, use_fpm);
	ret_b = ref_fax_class1_delete(cb);
	frees_b = harness_alloc.frees;
	live_b = harness_alloc.live;
	bad_b = harness_alloc.bad_free;

	diff_eq_int("return value, input %ld", ret_a, ret_b, input);
	diff_eq_int("return value is 1, input %ld", ret_a, 1, input);
	diff_eq_int("frees agree, input %ld", frees_a, frees_b, input);
	diff_eq_int("live, ours, input %ld", live_a, 0, input);
	diff_eq_int("live, blob, input %ld", live_b, 0, input);
	diff_eq_int("bad_free, ours, input %ld", bad_a, 0, input);
	diff_eq_int("bad_free, blob, input %ld", bad_b, 0, input);
}

static int
test_delete(void)
{
	diff_begin("fax_class1_delete");

	run_case(1, 1, 0);	/* RX data modem, tone present */
	run_case(1, 0, 1);	/* RX data modem, no tone      */
	run_case(0, 1, 2);	/* TX data modem, tone present */
	run_case(0, 0, 3);	/* TX data modem, no tone      */

	return diff_end();
}

/* ------------------------------------------------------------------- */

static void
run_scrm_ones(struct fax_class1 *ctx, int input)
{
	short tx_a[256], tx_b[256];
	int tcnt_a, tcnt_b, w8_a, w8_b;
	int ra, rb;
	struct fax_class1 cb;

	memset(&cb, 0, sizeof(cb));
	cb.countdown = ctx->countdown;
	cb.silence_blocks = ctx->silence_blocks;
	cb.state = ctx->state;

	memset(tx_a, 0x5a, sizeof(tx_a));
	memset(tx_b, 0x5a, sizeof(tx_b));
	tcnt_a = tcnt_b = -1;
	w8_a = w8_b = -1;

	ra = _tx_silence_before_scrm_ones(ctx, NULL, tx_a, 0, 0, NULL,
					  &tcnt_a, 0, &w8_a);
	rb = ref__tx_silence_before_scrm_ones(&cb, NULL, tx_b, 0, 0, NULL,
					      &tcnt_b, 0, &w8_b);

	diff_eq_int("scrm_ones: return, input %d", ra, rb, input);
	diff_eq_int("scrm_ones: countdown, input %d", ctx->countdown,
		    cb.countdown, input);
	diff_eq_int("scrm_ones: state, input %d", ctx->state, cb.state, input);
	diff_eq_int("scrm_ones: tx_count, input %d", tcnt_a, tcnt_b, input);
	diff_eq_int("scrm_ones: tx_count is 160, input %d", tcnt_a, 160,
		    input);
	diff_eq_int("scrm_ones: word8, input %d", w8_a, w8_b, input);
	diff_eq_int("scrm_ones: word8 is 0, input %d", w8_a, 0, input);
	diff_eq_int("scrm_ones: tx buf matches, input %d",
		    memcmp(tx_a, tx_b, sizeof(tx_a)), 0, input);
}

static int
test_scrm_ones(void)
{
	int i;
	static const struct { int countdown; unsigned int silence_blocks; }
	cases[] = {
		{ 0, 100 },		/* far below: no transition   */
		{ 79, 100 },		/* one block short             */
		{ 80, 100 },		/* exactly reaches (after +20) */
		{ 200, 50 },		/* already past                */
		{ -50, 10 },		/* negative countdown          */
	};

	diff_begin("_tx_silence_before_scrm_ones");

	for (i = 0; i < (int)(sizeof(cases) / sizeof(cases[0])); i++) {
		unsigned level;

		for (level = 0; level < 3; level++) {
			struct fax_class1 ctx;

			memset(&ctx, 0, sizeof(ctx));
			ctx.countdown = cases[i].countdown;
			ctx.silence_blocks = cases[i].silence_blocks;
			ctx.state = CLASS1_TX_SILENCE_BEFORE_SCRM_ONES;
			dsplibs_debug_level = ref_dsplibs_debug_level = level;

			run_scrm_ones(&ctx, (int)(level * 100 + (unsigned)i));
		}
	}
	dsplibs_debug_level = ref_dsplibs_debug_level = 0;

	return diff_end();
}

/* ------------------------------------------------------------------- */

static void
run_t30(struct fax_class1 *ctx, int input)
{
	short tx_a[256], tx_b[256];
	int tcnt_a, tcnt_b;
	int ra, rb;
	struct fax_class1 cb;

	memset(&cb, 0, sizeof(cb));
	cb.countdown = ctx->countdown;
	cb.clock_sec = ctx->clock_sec;
	cb.clock_frac = ctx->clock_frac;
	cb.state = ctx->state;
	cb.status = ctx->status;

	memset(tx_a, 0x5a, sizeof(tx_a));
	memset(tx_b, 0x5a, sizeof(tx_b));
	tcnt_a = tcnt_b = -1;

	ra = _t30_silence_before_tx_state(ctx, NULL, tx_a, 0, 0, NULL,
					  &tcnt_a, 0, NULL);
	rb = ref__t30_silence_before_tx_state(&cb, NULL, tx_b, 0, 0, NULL,
					      &tcnt_b, 0, NULL);

	diff_eq_int("t30: return, input %d", ra, rb, input);
	diff_eq_int("t30: countdown, input %d", ctx->countdown, cb.countdown,
		    input);
	diff_eq_int("t30: state, input %d", ctx->state, cb.state, input);
	diff_eq_int("t30: status, input %d", ctx->status, cb.status, input);
	diff_eq_int("t30: tx_count, input %d", tcnt_a, tcnt_b, input);
	diff_eq_int("t30: tx_count is 160, input %d", tcnt_a, 160, input);
	diff_eq_int("t30: tx buf matches, input %d",
		    memcmp(tx_a, tx_b, sizeof(tx_a)), 0, input);
}

static int
test_t30(void)
{
	int i;
	static const int counts[] = { 0, 1, 399, 400, 401, 1000, -5 };

	diff_begin("_t30_silence_before_tx_state");

	for (i = 0; i < (int)(sizeof(counts) / sizeof(counts[0])); i++) {
		unsigned level;

		for (level = 0; level < 3; level++) {
			struct fax_class1 ctx;

			memset(&ctx, 0, sizeof(ctx));
			ctx.countdown = counts[i];
			ctx.clock_sec = 12;
			ctx.clock_frac = 34;
			ctx.state = CLASS1_T30_SILENCE_BEFORE_PREAMBLE_STATE;
			ctx.status = FAX_CLASS1_NO_MESSAGE;
			dsplibs_debug_level = ref_dsplibs_debug_level = level;

			run_t30(&ctx, (int)(level * 100 + (unsigned)i));
		}
	}
	dsplibs_debug_level = ref_dsplibs_debug_level = 0;

	return diff_end();
}

/* ------------------------------------------------------------------- */

int
main(void)
{
	int rc = 0;

	rc |= test_delete();
	rc |= test_scrm_ones();
	rc |= test_t30();

	return rc;
}

/*
 * t_class1delmodem.c -- differential test of `_delete_data_rx_modem`
 * (class1rx.c) and `_delete_data_tx_modem` (class1tx.c).
 *
 * BOTH ARE TESTED VIA THE ALLOCATION LOG (t_faxvmids.c's technique), because
 * the memory they free is undefined the moment it is freed.  `ctx->vmi_b`
 * is a hand-built `struct faxvmi` with `slot == 0` (the NULL dispatch slot),
 * independent of which modulation `ctx->modem_vmi` holds -- these are TWO
 * DIFFERENT structs with two different `slot` fields (`struct faxvmi`'s own
 * and `struct faxvmi_cfg`'s), and only the first is `FAXVMI_delete`'s own
 * dispatch key.  A real per-modulation `vxx_delete` entry is not proven safe
 * against a synthetic instance this wave, so slot 0 keeps that half of the
 * picture on ground `t_faxvmids.c` already proved (7 frees, 0 live, 0
 * bad_free).
 *
 * `ctx->modem_vmi` is built with the REAL constructors -- `init_vmi_v17rx`/
 * `v27rx`/`v29rx` and `init_vmi_v17tx`/`v27tx`/`v29tx`, already differentially
 * tested in `t_faxcfg.c` and this wave -- so the allocation graph the delete
 * functions walk is exactly what `_init_receiver`/`_init_transmitter` would
 * have built, not a fixture invented for this file.
 */

#include <stddef.h>
#include <string.h>

#include "harness.h"
#include "dsplib/class1.h"
#include "dsplib/class1rx.h"
#include "dsplib/class1tx.h"
#include "dsplib/faxcfg.h"
#include "dsplib/faxfifo.h"
#include "dsplib/faxvmi.h"
#include "dsplib/sysdep.h"

extern void ref__delete_data_rx_modem(struct fax_class1 *ctx);
extern void ref__delete_data_tx_modem(struct fax_class1 *ctx);

extern void ref_init_vmi_v17rx(struct faxvmi_cfg *vmi, unsigned short bit_rate,
			       int arg_2, void *arg_3);
extern void ref_init_vmi_v27rx(struct faxvmi_cfg *vmi, unsigned short bit_rate,
			       int arg_2, void *arg_3);
extern void ref_init_vmi_v29rx(struct faxvmi_cfg *vmi, unsigned short bit_rate,
			       int arg_2, void *arg_3);
extern int ref_init_vmi_v17tx(struct faxvmi_cfg *vmi, unsigned short bit_rate,
			      int arg_2, void *arg_3);
extern int ref_init_vmi_v27tx(struct faxvmi_cfg *vmi, unsigned short bit_rate,
			      int arg_2, void *arg_3);
extern int ref_init_vmi_v29tx(struct faxvmi_cfg *vmi, unsigned short bit_rate,
			      int arg_2, void *arg_3);
extern struct fax_fifo *ref_FIFO_create(struct fax_fifo *f,
					const struct fifo_cfg *cfg);

/*
 * A NULL-slot `struct faxvmi`, with the full allocation graph
 * `FAXVMI_delete` frees regardless of modulation: `link->ptr_0000`,
 * `link->buf`, `framer->fifo`, `framer->frame`, `link`, `framer`, `vmi`
 * itself -- seven pointers, exactly `t_faxvmids.c`'s own count.
 */
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

static int marker;

/* ------------------------------------------------------------------ */

typedef void (*init_rx_fn)(struct faxvmi_cfg *, unsigned short, int, void *);
typedef int (*init_tx_fn)(struct faxvmi_cfg *, unsigned short, int, void *);

/*
 * BOTH THE BUILD AND THE DELETE MUST SHARE ONE `harness_alloc_reset()`
 * WINDOW.  `harness_alloc` tracks a LIVE SET of allocations made since the
 * last reset; resetting after building (and before deleting) forgets the
 * very pointers the delete function is about to free, which reads back as
 * every free being a "bad" one on a pointer the log never saw -- not a
 * property of `_delete_data_rx_modem`/`_delete_data_tx_modem` at all.  So
 * each side's build AND delete run inside one reset, exactly as
 * `t_faxvmids.c`'s own `run_delete` does.
 */
static void
run_rx_case(const char *name, init_rx_fn init, init_rx_fn ref_init,
	    unsigned short bit_rate, long expect_frees, long input)
{
	struct fax_class1 ctx_a, ctx_b;
	long tag = input;

	memset(&ctx_a, 0, sizeof(ctx_a));
	memset(&ctx_b, 0, sizeof(ctx_b));

	harness_alloc_reset();
	ctx_a.modem_vmi = sysdep_malloc(sizeof(struct faxvmi_cfg));
	init(ctx_a.modem_vmi, bit_rate, 0x1234, &marker);
	ctx_a.vmi_b = make_handle();
	_delete_data_rx_modem(&ctx_a);
	diff_eq_int("frees, ours (%ld)", harness_alloc.frees, expect_frees,
		    tag);
	diff_eq_int("live, ours (%ld)", harness_alloc.live, 0, tag);
	diff_eq_int("bad_free, ours (%ld)", harness_alloc.bad_free, 0, tag);
	diff_eq_int("modem_vmi cleared, ours (%ld)",
		    ctx_a.modem_vmi == NULL, 1, tag);
	diff_eq_int("vmi_b cleared, ours (%ld)", ctx_a.vmi_b == NULL, 1, tag);

	harness_alloc_reset();
	ctx_b.modem_vmi = sysdep_malloc(sizeof(struct faxvmi_cfg));
	ref_init(ctx_b.modem_vmi, bit_rate, 0x1234, &marker);
	ctx_b.vmi_b = make_handle();
	ref__delete_data_rx_modem(&ctx_b);
	diff_eq_int("frees, blob (%ld)", harness_alloc.frees, expect_frees,
		    tag);
	diff_eq_int("live, blob (%ld)", harness_alloc.live, 0, tag);
	diff_eq_int("bad_free, blob (%ld)", harness_alloc.bad_free, 0, tag);
	diff_eq_int("modem_vmi cleared, blob (%ld)",
		    ctx_b.modem_vmi == NULL, 1, tag);
	diff_eq_int("vmi_b cleared, blob (%ld)", ctx_b.vmi_b == NULL, 1, tag);

	(void)name;
}

static int
test_delete_rx(void)
{
	diff_begin("_delete_data_rx_modem");

	/* V.17: 3 sub-allocations + cfg + vmi block + FAXVMI_delete's 7. */
	run_rx_case("v17rx", init_vmi_v17rx, ref_init_vmi_v17rx, 14400,
		    3 + 1 + 1 + 7, 0);
	run_rx_case("v17rx/9600", init_vmi_v17rx, ref_init_vmi_v17rx, 9600,
		    3 + 1 + 1 + 7, 1);

	/* V.27ter / V.29: cfg + vmi block + FAXVMI_delete's 7, no subs. */
	run_rx_case("v27rx", init_vmi_v27rx, ref_init_vmi_v27rx, 4800,
		    1 + 1 + 7, 2);
	run_rx_case("v29rx", init_vmi_v29rx, ref_init_vmi_v29rx, 9600,
		    1 + 1 + 7, 3);

	return diff_end();
}

/* ------------------------------------------------------------------ */

static void
run_tx_case(const char *name, init_tx_fn init, init_tx_fn ref_init,
	    unsigned short bit_rate, int with_fifo, long input)
{
	struct fax_class1 ctx_a, ctx_b;
	long tag = input;
	long expect_frees = 1 /* cfg */ + 1 /* vmi block */ + 7 /* FAXVMI_delete */
			   + (with_fifo ? 2 : 0);

	memset(&ctx_a, 0, sizeof(ctx_a));
	memset(&ctx_b, 0, sizeof(ctx_b));

	harness_alloc_reset();
	ctx_a.modem_vmi = sysdep_malloc(sizeof(struct faxvmi_cfg));
	init(ctx_a.modem_vmi, bit_rate, 0x1234, &marker);
	ctx_a.vmi_b = make_handle();
	ctx_a.tx_fifo = with_fifo ? FIFO_create(NULL, NULL) : NULL;
	_delete_data_tx_modem(&ctx_a);
	diff_eq_int("frees, ours (%ld)", harness_alloc.frees, expect_frees,
		    tag);
	diff_eq_int("live, ours (%ld)", harness_alloc.live, 0, tag);
	diff_eq_int("bad_free, ours (%ld)", harness_alloc.bad_free, 0, tag);
	diff_eq_int("modem_vmi cleared, ours (%ld)",
		    ctx_a.modem_vmi == NULL, 1, tag);
	diff_eq_int("vmi_b cleared, ours (%ld)", ctx_a.vmi_b == NULL, 1, tag);
	diff_eq_int("tx_fifo cleared, ours (%ld)", ctx_a.tx_fifo == NULL, 1, tag);

	harness_alloc_reset();
	ctx_b.modem_vmi = sysdep_malloc(sizeof(struct faxvmi_cfg));
	ref_init(ctx_b.modem_vmi, bit_rate, 0x1234, &marker);
	ctx_b.vmi_b = make_handle();
	ctx_b.tx_fifo = with_fifo ? ref_FIFO_create(NULL, NULL) : NULL;
	ref__delete_data_tx_modem(&ctx_b);
	diff_eq_int("frees, blob (%ld)", harness_alloc.frees, expect_frees,
		    tag);
	diff_eq_int("live, blob (%ld)", harness_alloc.live, 0, tag);
	diff_eq_int("bad_free, blob (%ld)", harness_alloc.bad_free, 0, tag);
	diff_eq_int("modem_vmi cleared, blob (%ld)",
		    ctx_b.modem_vmi == NULL, 1, tag);
	diff_eq_int("vmi_b cleared, blob (%ld)", ctx_b.vmi_b == NULL, 1, tag);
	diff_eq_int("tx_fifo cleared, blob (%ld)", ctx_b.tx_fifo == NULL, 1, tag);

	(void)name;
}

static int
test_delete_tx(void)
{
	diff_begin("_delete_data_tx_modem");

	run_tx_case("v17tx, no fifo", init_vmi_v17tx, ref_init_vmi_v17tx,
		    14400, 0, 0);
	run_tx_case("v17tx, fifo", init_vmi_v17tx, ref_init_vmi_v17tx,
		    9600, 1, 1);
	run_tx_case("v27tx, no fifo", init_vmi_v27tx, ref_init_vmi_v27tx,
		    4800, 0, 2);
	run_tx_case("v27tx, fifo", init_vmi_v27tx, ref_init_vmi_v27tx,
		    2400, 1, 3);
	run_tx_case("v29tx, no fifo", init_vmi_v29tx, ref_init_vmi_v29tx,
		    9600, 0, 4);
	run_tx_case("v29tx, fifo", init_vmi_v29tx, ref_init_vmi_v29tx,
		    7200, 1, 5);

	return diff_end();
}

/* ------------------------------------------------------------------ */

int
main(void)
{
	int rc = 0;

	rc |= test_delete_rx();
	rc |= test_delete_tx();

	return rc;
}

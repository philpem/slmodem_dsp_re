/*
 * t_class1txcplinit.c -- differential test of the `class1tx.c` leaves that
 * were blocked on `FAXVMI_control`: `_rx_look_carrier_init`,
 * `_tx_scrambled_ones_init`, `cHDLCtx_preamble_state_init`,
 * `_cHDLCrx_init_from_idle` and `cHDLCtx_off_init`.  See class1tx.c and
 * docs/findings.md for the per-function derivation.
 *
 * `_rx_look_carrier_init`/`_tx_scrambled_ones_init` drive the already-tested
 * `_init_receiver`/`_init_transmitter` (t_class1initrx.c/t_class1inittx.c),
 * so their own cases build a fresh data modem the same way those files do.
 *
 * `cHDLCtx_preamble_state_init`/`_cHDLCrx_init_from_idle`/`cHDLCtx_off_init`
 * drive `FAXVMI_control` on `vmi_c`/`vmi_a`, the V.21 TX/RX control-channel
 * handles -- built here the same way `fax_class1_create` builds them (a
 * `faxvmi_cfg` wrapping a `V21TX_CFG`/`V21RX_CFG`-based modem config, slot
 * `VMI_SLOT_V21TX`/`VMI_SLOT_V21RX`, through `FAXVMI_create`), since neither
 * this file nor `fax_class1_create` is a precondition for the other -- V.21's
 * own constructors/controllers are already fully reconstructed and tested
 * (t_v21create.c, t_v21fax.c).
 *
 * `cHDLCtx_off_init` is different from the other four in one way worth
 * flagging: no entry point in the object reaches it (F8320's no-entry-point
 * bucket), so there is no dispatcher or caller to drive it through -- its
 * case calls `ref_cHDLCtx_off_init`/`cHDLCtx_off_init` directly, the same way
 * every no-entry-point leaf in this tree is tested.
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
#include "dsplib/v21cfg.h"
#include "dsplib/v21fax.h"

extern int ref__rx_look_carrier_init(struct fax_class1 *ctx, int rate_code);
extern int ref__tx_scrambled_ones_init(struct fax_class1 *ctx, int rate_code);
extern int ref_cHDLCtx_preamble_state_init(struct fax_class1 *ctx);
extern int ref__cHDLCrx_init_from_idle(struct fax_class1 *ctx, int arg2);
extern int ref_cHDLCtx_off_init(struct fax_class1 *ctx);
extern unsigned int ref_dsplibs_debug_level;

/* Build a real V.21 TX handle, the same shape fax_class1_create uses. */
static struct faxvmi *
make_vmi_c(void)
{
	struct faxvmi_cfg cfg = FAXVMI_CFG;
	struct v21tx_cfg tx_cfg = V21TX_CFG;

	cfg.short_0000 = 2;
	cfg.int_0004 = 1;
	cfg.short_0008 = 0x60;
	cfg.short_000a = 0x34;
	cfg.short_000c = 0;
	cfg.slot = VMI_SLOT_V21TX;
	cfg.modem_cfg = &tx_cfg;
	return FAXVMI_create(NULL, &cfg);
}

/* Build a real V.21 RX handle, the same shape fax_class1_create uses. */
static struct faxvmi *
make_vmi_a(void)
{
	struct faxvmi_cfg cfg = FAXVMI_CFG;
	struct v21rx_cfg rx_cfg = V21RX_CFG;

	cfg.short_0000 = 2;
	cfg.int_0004 = 1;
	cfg.short_0008 = 0;
	cfg.short_000a = 0x60;
	cfg.short_000c = 0x60;
	cfg.slot = VMI_SLOT_V21RX;
	cfg.modem_cfg = &rx_cfg;
	return FAXVMI_create(NULL, &cfg);
}

static void
cmp_ctx_common(const char *what, struct fax_class1 *a, struct fax_class1 *b,
	       long tag)
{
	char buf[64];

#define FLD(name) \
	do { \
		(void)snprintf(buf, sizeof(buf), "%s.%s (%%ld)", what, #name); \
		diff_eq_int(buf, b->name, a->name, tag); \
	} while (0)
	FLD(state);
	FLD(countdown);
	FLD(delayed_status_countdown);
	FLD(current_mod);
	FLD(tx_rate);
	FLD(tx_connect_countdown);
	FLD(tx_bytes_per_block);
	FLD(tx_connect_latch);
	FLD(transmit_enabled);
	FLD(tx_fifo_ready);
	FLD(data_input_closed);
	FLD(hdlc_frame_done);
	FLD(buffers_sent);
	FLD(frame_end_latch);
	FLD(hdlc_write_cursor);
#undef FLD
}

/* _rx_look_carrier_init: fresh data-mode receiver build, all three mods. */
static int
run_rx_look_carrier(int rate_code, long tag)
{
	struct fax_class1 ctx_a, ctx_b;
	int ra, rb;

	memset(&ctx_a, 0, sizeof(ctx_a));
	memset(&ctx_b, 0, sizeof(ctx_b));
	ctx_a.async_locked = ctx_b.async_locked = 1;
	ctx_a.async_window = ctx_b.async_window = 0x12345678U;
	ctx_a.countdown = ctx_b.countdown = 77;

	diff_begin("_rx_look_carrier_init");

	ra = ref__rx_look_carrier_init(&ctx_a, rate_code);
	rb = _rx_look_carrier_init(&ctx_b, rate_code);
	diff_eq_int("_rx_look_carrier_init ret (%ld)", rb, ra, tag);
	cmp_ctx_common("rx_look_carrier", &ctx_a, &ctx_b, tag);
	diff_eq_int("async_locked (%ld)", ctx_b.async_locked, ctx_a.async_locked,
		    tag);
	diff_eq_int("async_window (%ld)", (int)ctx_b.async_window,
		    (int)ctx_a.async_window, tag);

	return diff_end();
}

/* _tx_scrambled_ones_init: fresh data-mode transmitter build. */
static int
run_tx_scrambled_ones(int rate_code, long tag)
{
	struct fax_class1 ctx_a, ctx_b;
	int ra, rb;

	memset(&ctx_a, 0, sizeof(ctx_a));
	memset(&ctx_b, 0, sizeof(ctx_b));
	ctx_a.tx_connect_countdown = ctx_b.tx_connect_countdown = 999;
	ctx_a.transmit_enabled = ctx_b.transmit_enabled = 1;

	diff_begin("_tx_scrambled_ones_init");

	ra = ref__tx_scrambled_ones_init(&ctx_a, rate_code);
	rb = _tx_scrambled_ones_init(&ctx_b, rate_code);
	diff_eq_int("_tx_scrambled_ones_init ret (%ld)", rb, ra, tag);
	cmp_ctx_common("tx_scrambled_ones", &ctx_a, &ctx_b, tag);

	return diff_end();
}

/* Re-running _tx_scrambled_ones_init on an already-built FIFO. */
static int
run_tx_scrambled_ones_again(int rate_code, long tag)
{
	struct fax_class1 ctx_a, ctx_b;

	memset(&ctx_a, 0, sizeof(ctx_a));
	memset(&ctx_b, 0, sizeof(ctx_b));

	(void)ref__tx_scrambled_ones_init(&ctx_a, rate_code);
	(void)_tx_scrambled_ones_init(&ctx_b, rate_code);

	diff_begin("_tx_scrambled_ones_init: again");

	(void)ref__tx_scrambled_ones_init(&ctx_a, rate_code);
	(void)_tx_scrambled_ones_init(&ctx_b, rate_code);
	cmp_ctx_common("tx_scrambled_ones again", &ctx_a, &ctx_b, tag);

	return diff_end();
}

/* cHDLCtx_preamble_state_init: real vmi_c, fresh and reinit. */
static int
run_preamble_init(long tag)
{
	struct fax_class1 ctx_a, ctx_b;
	int ra, rb;

	memset(&ctx_a, 0, sizeof(ctx_a));
	memset(&ctx_b, 0, sizeof(ctx_b));
	ctx_a.vmi_c = make_vmi_c();
	ctx_b.vmi_c = make_vmi_c();
	ctx_a.state = ctx_b.state = CLASS1_ANSWER_TONE_STATE;
	ctx_a.frame_end_latch = ctx_b.frame_end_latch = 1;
	ctx_a.hdlc_write_cursor = ctx_b.hdlc_write_cursor = 9;
	ctx_a.hdlc_frame_done = ctx_b.hdlc_frame_done = 1;
	ctx_a.buffers_sent = ctx_b.buffers_sent = 3;

	diff_begin("cHDLCtx_preamble_state_init");

	ra = ref_cHDLCtx_preamble_state_init(&ctx_a);
	rb = cHDLCtx_preamble_state_init(&ctx_b);
	diff_eq_int("cHDLCtx_preamble_state_init ret (%ld)", rb, ra, tag);
	cmp_ctx_common("preamble_init", &ctx_a, &ctx_b, tag);

	/* A second call proves the REINIT path (already-built vmi_c). */
	ra = ref_cHDLCtx_preamble_state_init(&ctx_a);
	rb = cHDLCtx_preamble_state_init(&ctx_b);
	diff_eq_int("cHDLCtx_preamble_state_init reinit ret (%ld)", rb, ra,
		    tag + 1);
	cmp_ctx_common("preamble_init reinit", &ctx_a, &ctx_b, tag + 1);

	return diff_end();
}

/* _cHDLCrx_init_from_idle: real vmi_a, both branches of arg2. */
static int
run_hdlcrx_init(int arg2, long tag)
{
	struct fax_class1 ctx_a, ctx_b;
	int ra, rb;

	memset(&ctx_a, 0, sizeof(ctx_a));
	memset(&ctx_b, 0, sizeof(ctx_b));
	ctx_a.vmi_a = make_vmi_a();
	ctx_b.vmi_a = make_vmi_a();
	ctx_a.state = ctx_b.state = CLASS1_IDLE_STATE;
	ctx_a.countdown = ctx_b.countdown = 55;
	ctx_a.delayed_status_countdown = ctx_b.delayed_status_countdown = 66;

	diff_begin("_cHDLCrx_init_from_idle");

	ra = ref__cHDLCrx_init_from_idle(&ctx_a, arg2);
	rb = _cHDLCrx_init_from_idle(&ctx_b, arg2);
	diff_eq_int("_cHDLCrx_init_from_idle ret (%ld)", rb, ra, tag);
	cmp_ctx_common("hdlcrx_init", &ctx_a, &ctx_b, tag);

	return diff_end();
}

/*
 * cHDLCtx_off_init: no entry point in the object reaches it (F8320's
 * no-entry-point bucket -- see class1tx.h), so unlike every other case in
 * this file it is called directly through its `ref_` alias rather than
 * through any dispatcher or caller. Same `vmi_a` build as
 * `run_hdlcrx_init` above, since it drives the same `ctx->vmi_a` handle.
 */
static int
run_cHDLCtx_off_init(long tag)
{
	struct fax_class1 ctx_a, ctx_b;
	int ra, rb;

	memset(&ctx_a, 0, sizeof(ctx_a));
	memset(&ctx_b, 0, sizeof(ctx_b));
	ctx_a.vmi_a = make_vmi_a();
	ctx_b.vmi_a = make_vmi_a();
	ctx_a.state = ctx_b.state = CLASS1_IDLE_STATE;
	ctx_a.countdown = ctx_b.countdown = 55;
	ctx_a.delayed_status_countdown = ctx_b.delayed_status_countdown = 66;

	diff_begin("cHDLCtx_off_init");

	ra = ref_cHDLCtx_off_init(&ctx_a);
	rb = cHDLCtx_off_init(&ctx_b);
	diff_eq_int("cHDLCtx_off_init ret (%ld)", rb, ra, tag);
	cmp_ctx_common("cHDLCtx_off_init", &ctx_a, &ctx_b, tag);

	/* A second call proves the REINIT path (already-built vmi_a). */
	ra = ref_cHDLCtx_off_init(&ctx_a);
	rb = cHDLCtx_off_init(&ctx_b);
	diff_eq_int("cHDLCtx_off_init reinit ret (%ld)", rb, ra, tag + 1);
	cmp_ctx_common("cHDLCtx_off_init reinit", &ctx_a, &ctx_b, tag + 1);

	return diff_end();
}

/* Debug-level coverage across all four -- proves the gated prints agree. */
static int
run_debug_on(void)
{
	struct fax_class1 ctx_a, ctx_b;
	int rc = 0;

	dsplibs_debug_level = ref_dsplibs_debug_level = 2;

	memset(&ctx_a, 0, sizeof(ctx_a));
	memset(&ctx_b, 0, sizeof(ctx_b));
	ctx_a.vmi_a = make_vmi_a();
	ctx_b.vmi_a = make_vmi_a();

	diff_begin("debug_level > 1: _cHDLCrx_init_from_idle");
	(void)ref__cHDLCrx_init_from_idle(&ctx_a, 3);
	(void)_cHDLCrx_init_from_idle(&ctx_b, 3);
	cmp_ctx_common("debug hdlcrx", &ctx_a, &ctx_b, 900);
	rc |= diff_end();

	memset(&ctx_a, 0, sizeof(ctx_a));
	memset(&ctx_b, 0, sizeof(ctx_b));

	diff_begin("debug_level > 1: _tx_scrambled_ones_init");
	(void)ref__tx_scrambled_ones_init(&ctx_a, 0x18);
	(void)_tx_scrambled_ones_init(&ctx_b, 0x18);
	cmp_ctx_common("debug tx_scrambled", &ctx_a, &ctx_b, 901);
	rc |= diff_end();

	dsplibs_debug_level = ref_dsplibs_debug_level = 0;
	return rc;
}

int
main(void)
{
	int rc = 0;

	rc |= run_rx_look_carrier(0x18, 10);	/* V.27ter 2400 */
	rc |= run_rx_look_carrier(0x60, 11);	/* V.29 9600 */
	rc |= run_rx_look_carrier(0x91, 12);	/* V.17 14400 */

	rc |= run_tx_scrambled_ones(0x18, 20);
	rc |= run_tx_scrambled_ones(0x48, 21);
	rc |= run_tx_scrambled_ones(0x92, 22);
	rc |= run_tx_scrambled_ones_again(0x30, 23);

	rc |= run_preamble_init(30);

	rc |= run_hdlcrx_init(3, 40);		/* the CLASS1_HDLC_RECEIVE_LOOK_CARRIER
						 * sentinel */
	rc |= run_hdlcrx_init(0x18, 41);	/* a real rate code, never 3 */
	rc |= run_hdlcrx_init(0, 42);

	rc |= run_cHDLCtx_off_init(50);

	rc |= run_debug_on();

	return rc;
}

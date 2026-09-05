/*
 * t_class1initrx.c -- differential test of `_init_receiver` (class1rx.c).
 *
 * UNLIKE t_faxvmicp.c's own creates, THIS test drives real per-modulation
 * slots (V.17/V.27ter/V.29 receive) rather than the NULL ones: exercising
 * `_init_receiver`'s own job means going through its own dispatch tables
 * (`init_vmi_data_rx_modem`, `FAXVMI_create`'s own `vxx_create[slot]`), which
 * land on `V17RX_create`/`V27RX_create`/`V29RX_create` -- already
 * differentially tested elsewhere in this tree and routinely invoked as
 * `vxx_create` table entries, so this is not a fresh dependency, just a
 * fuller one than t_faxvmicp.c chose to take on.
 *
 * THE WRAPPED-OBJECT POKES (`ctx->rx_agc_mult`/`rx_agc_shift`/`f12d4` propagated into an
 * unmodelled per-modulation sub-object reached via `vmi_b->link->int_0014`)
 * are verified the same way: both sides call the IDENTICAL already-written
 * constructor, so the sub-object's shape and size are identical on both
 * sides by construction, and reading the same computed offset back on both
 * is a safe, meaningful comparison even with no struct named for it.
 *
 * The f12d4-propagation offset (`off1_d4`) is always four less than the
 * rx_agc_mult/rx_agc_shift one (`off1_c0c4`) -- 0x54/0x50 (V.27ter), 0x50/0x4c (V.29),
 * 0x60/0x5c (V.17) -- read straight off `dis.py`, not a guessed pattern.
 */

#include <stddef.h>
#include <string.h>

#include "harness.h"
#include "dsplib/class1.h"
#include "dsplib/class1rx.h"
#include "dsplib/debug.h"
#include "dsplib/faxcfg.h"
#include "dsplib/faxvmi.h"

extern void ref__init_receiver(struct fax_class1 *ctx, int rate_code);
extern unsigned int ref_dsplibs_debug_level;

/* got=ours (b), want=blob (a), t_faxvmicp.c's own convention */
static void
cmp_cfg_scalars(const char *what, struct faxvmi_cfg *a, struct faxvmi_cfg *b,
		long tag)
{
	char buf[64];

#define FLD(name) \
	do { \
		(void)snprintf(buf, sizeof(buf), "%s.%s (%%ld)", what, #name); \
		diff_eq_int(buf, b->name, a->name, tag); \
	} while (0)
	FLD(mode);
	FLD(short_0002);
	FLD(reverse);
	FLD(fifo_size);
	FLD(max_frame);
	FLD(frame_size);
	FLD(slot);
#undef FLD
}

static void
cmp_vmi_scalars(const char *what, struct faxvmi *a, struct faxvmi *b,
		long tag)
{
	char buf[64];

#define FLD(name) \
	do { \
		(void)snprintf(buf, sizeof(buf), "%s.%s (%%ld)", what, #name); \
		diff_eq_int(buf, b->name, a->name, tag); \
	} while (0)
	FLD(mode);
	FLD(reverse);
	FLD(fifo_size);
	FLD(max_frame);
	FLD(frame_size);
	FLD(slot);
	FLD(underrun);
	FLD(overflow);
	FLD(status);
#undef FLD
}

static void
cmp_framer(const char *what, struct faxvmi_framer *a, struct faxvmi_framer *b,
	   long tag)
{
	struct faxvmi_framer ca = *a, cb = *b;

	ca.fifo = cb.fifo = NULL;
	ca.frame = cb.frame = NULL;
	diff_eq_obj(what, struct faxvmi_framer, &cb, &ca, tag);
}

static void
cmp_link_scalars(const char *what, struct faxvmi_link *a,
		 struct faxvmi_link *b, long tag)
{
	char buf[64];

#define FLD(name) \
	do { \
		(void)snprintf(buf, sizeof(buf), "%s.%s (%%ld)", what, #name); \
		diff_eq_int(buf, b->name, a->name, tag); \
	} while (0)
	FLD(pack_count);
	FLD(pack_width);
	FLD(unpack_width);
#undef FLD
}

/*
 * Read the SAME computed offset back on both sides.  `off1` is the offset
 * of the sub-pointer inside the wrapped modem object (`vmi->link->
 * int_0014`, cast to a pointer, the same idiom `faxadapt.c`'s own `FIELD_
 * PTR` macro uses on this exact field); `off2` is the offset of the 16-bit
 * value inside THAT sub-object.  Neither object is named anywhere in this
 * tree, so this reads raw bytes rather than through a struct -- safe here
 * because both sides ran the identical already-tested constructor.
 */
static short
wrapped_short(struct faxvmi *vmi, int off1, int off2)
{
	void *wrapped = (void *)(long)vmi->link->int_0014;
	void *sub = *(void **)((char *)wrapped + off1);

	return *(short *)((char *)sub + off2);
}

static void
cmp_ctx(const char *what, struct fax_class1 *a, struct fax_class1 *b,
       long tag)
{
	char buf[64];

	(void)snprintf(buf, sizeof(buf), "%s current_mod (%%ld)", what);
	diff_eq_int(buf, b->current_mod, a->current_mod, tag);
	(void)snprintf(buf, sizeof(buf), "%s state (%%ld)", what);
	diff_eq_int(buf, b->state, a->state, tag);

	cmp_cfg_scalars("modem_vmi", a->modem_vmi, b->modem_vmi, tag);
	cmp_vmi_scalars("vmi_b", a->vmi_b, b->vmi_b, tag);
	cmp_framer("vmi_b framer", a->vmi_b->framer, b->vmi_b->framer, tag);
	cmp_link_scalars("vmi_b link", a->vmi_b->link, b->vmi_b->link, tag);
}

/*
 * One fresh-create case: zeroed ctx, `ctx->rx_agc_mult`/`rx_agc_shift`/`f12d4` seeded to
 * a distinct nonzero pattern (so a propagation bug shows as a real
 * mismatch, not two sides agreeing on zero), `_init_receiver` called once.
 */
static int
run_fresh(int rate_code, int off1_c0c4, int off2_c0c4, int off2_d4, long tag)
{
	struct fax_class1 ctx_a, ctx_b;
	int off1_d4 = off1_c0c4 - 4;

	memset(&ctx_a, 0, sizeof(ctx_a));
	memset(&ctx_b, 0, sizeof(ctx_b));
	ctx_a.rx_agc_mult = ctx_b.rx_agc_mult = (int)(0x1100 + tag);
	ctx_a.rx_agc_shift = ctx_b.rx_agc_shift = (int)(0x2200 + tag);
	ctx_a.f12d4 = ctx_b.f12d4 = (int)(0x3300 + tag);
	ctx_a.clock_sec = ctx_b.clock_sec = 12;
	ctx_a.clock_frac = ctx_b.clock_frac = 34;

	diff_begin("_init_receiver: fresh create");

	ref__init_receiver(&ctx_a, rate_code);
	_init_receiver(&ctx_b, rate_code);

	cmp_ctx("fresh", &ctx_a, &ctx_b, tag);

	diff_eq_int("fresh rx_agc_mult propagated (%ld)",
		    wrapped_short(ctx_b.vmi_b, off1_c0c4, off2_c0c4),
		    wrapped_short(ctx_a.vmi_b, off1_c0c4, off2_c0c4), tag);
	diff_eq_int("fresh rx_agc_shift propagated (%ld)",
		    wrapped_short(ctx_b.vmi_b, off1_c0c4, off2_c0c4 + 2),
		    wrapped_short(ctx_a.vmi_b, off1_c0c4, off2_c0c4 + 2), tag);
	diff_eq_int("fresh f12d4 propagated (%ld)",
		    wrapped_short(ctx_b.vmi_b, off1_d4, off2_d4),
		    wrapped_short(ctx_a.vmi_b, off1_d4, off2_d4), tag);

	return diff_end();
}

/*
 * Reinit: call `_init_receiver` a second time with the SAME rate code, so
 * `ctx->current_mod == mod` already and the REINIT path (the merged CTL
 * template, `FAXVMI_control`) runs instead of a second fresh create.
 */
static int
run_reinit(int rate_code, int off1_c0c4, int off2_d4, long tag)
{
	struct fax_class1 ctx_a, ctx_b;
	int off1_d4 = off1_c0c4 - 4;

	memset(&ctx_a, 0, sizeof(ctx_a));
	memset(&ctx_b, 0, sizeof(ctx_b));
	ctx_a.rx_agc_mult = ctx_b.rx_agc_mult = (int)(0x1400 + tag);
	ctx_a.rx_agc_shift = ctx_b.rx_agc_shift = (int)(0x2500 + tag);
	ctx_a.clock_sec = ctx_b.clock_sec = 5;
	ctx_a.clock_frac = ctx_b.clock_frac = 6;

	ref__init_receiver(&ctx_a, rate_code);
	_init_receiver(&ctx_b, rate_code);

	/* scribble f12d4 differently before the reinit call, so its own
	 * propagation is what this case actually exercises              */
	ctx_a.f12d4 = ctx_b.f12d4 = (int)(0x6600 + tag);

	diff_begin("_init_receiver: reinit (same modulation)");

	ref__init_receiver(&ctx_a, rate_code);
	_init_receiver(&ctx_b, rate_code);

	cmp_ctx("reinit", &ctx_a, &ctx_b, tag);

	diff_eq_int("reinit f12d4 propagated (%ld)",
		    wrapped_short(ctx_b.vmi_b, off1_d4, off2_d4),
		    wrapped_short(ctx_a.vmi_b, off1_d4, off2_d4), tag);
	diff_eq_int("reinit unpack_width == sym_size(rate) (%ld)",
		    ctx_b.vmi_b->link->unpack_width,
		    ctx_a.vmi_b->link->unpack_width, tag);

	return diff_end();
}

/*
 * A modulation SWITCH: create fresh at one rate, then call again at a
 * DIFFERENT modulation's rate -- `ctx->current_mod != mod` and
 * `ctx->modem_vmi != NULL`, so the teardown-then-fresh-create arm runs.
 */
static int
run_switch(int rate_code_1, int rate_code_2, long tag)
{
	struct fax_class1 ctx_a, ctx_b;

	memset(&ctx_a, 0, sizeof(ctx_a));
	memset(&ctx_b, 0, sizeof(ctx_b));

	ref__init_receiver(&ctx_a, rate_code_1);
	_init_receiver(&ctx_b, rate_code_1);

	diff_begin("_init_receiver: modulation switch");

	ref__init_receiver(&ctx_a, rate_code_2);
	_init_receiver(&ctx_b, rate_code_2);

	cmp_ctx("switch", &ctx_a, &ctx_b, tag);

	return diff_end();
}

/*
 * Debug-level coverage: both entry-point prints (`Initializing RX modem
 * receiver`, `Short train...`) and the reinit/switch prints, at
 * `dsplibs_debug_level > 1`.  No assertion beyond "both sides still agree",
 * since the prints themselves are not captured here -- this exists to
 * prove the debug-gated branches do not diverge, not to check their text.
 */
static int
run_debug_on(void)
{
	struct fax_class1 ctx_a, ctx_b;
	int rc;

	dsplibs_debug_level = ref_dsplibs_debug_level = 2;

	memset(&ctx_a, 0, sizeof(ctx_a));
	memset(&ctx_b, 0, sizeof(ctx_b));

	diff_begin("_init_receiver: debug_level > 1");

	/* V.17 short-training code: both debug prints on this path */
	ref__init_receiver(&ctx_a, 0x92);
	_init_receiver(&ctx_b, 0x92);
	cmp_ctx("debug fresh v17-short", &ctx_a, &ctx_b, 900);

	/* reinit, same modulation: "Restarting existing RX modem" */
	ref__init_receiver(&ctx_a, 0x92);
	_init_receiver(&ctx_b, 0x92);
	cmp_ctx("debug reinit v17-short", &ctx_a, &ctx_b, 901);

	/* switch to V.29: "New RX Modem... Deleting previous existing one" */
	ref__init_receiver(&ctx_a, 0x60);
	_init_receiver(&ctx_b, 0x60);
	cmp_ctx("debug switch to v29", &ctx_a, &ctx_b, 902);

	rc = diff_end();

	dsplibs_debug_level = ref_dsplibs_debug_level = 0;
	return rc;
}

int
main(void)
{
	int rc = 0;

	/* V.27ter: fresh rx_agc_mult/rx_agc_shift at wrapped+0x54 -> +0x8c/+0x8e,
	 * f12d4 at wrapped+0x50 -> +0x14                                 */
	rc |= run_fresh(0x18, 0x54, 0x8c, 0x14, 10);	/* 2400 */
	rc |= run_fresh(0x30, 0x54, 0x8c, 0x14, 11);	/* 4800 */

	/* V.29: fresh rx_agc_mult/rx_agc_shift at wrapped+0x50 -> +0x88/+0x8a,
	 * f12d4 at wrapped+0x4c -> +0x1c                                 */
	rc |= run_fresh(0x48, 0x50, 0x88, 0x1c, 20);	/* 7200 */
	rc |= run_fresh(0x60, 0x50, 0x88, 0x1c, 21);	/* 9600 */

	/* V.17: fresh rx_agc_mult/rx_agc_shift at wrapped+0x60 -> +0xd8/+0xda,
	 * f12d4 at wrapped+0x5c -> +0x20                                 */
	rc |= run_fresh(0x91, 0x60, 0xd8, 0x20, 30);	/* 14400 long */
	rc |= run_fresh(0x92, 0x60, 0xd8, 0x20, 31);	/* 14400 short */
	rc |= run_fresh(0x61, 0x60, 0xd8, 0x20, 32);	/* 9600 long */

	rc |= run_reinit(0x18, 0x54, 0x14, 40);	/* V.27ter 2400 */
	rc |= run_reinit(0x30, 0x54, 0x14, 41);	/* V.27ter 4800 */
	rc |= run_reinit(0x48, 0x50, 0x1c, 42);	/* V.29 7200 */
	rc |= run_reinit(0x60, 0x50, 0x1c, 43);	/* V.29 9600 */
	rc |= run_reinit(0x91, 0x60, 0x20, 44);	/* V.17 14400 */
	rc |= run_reinit(0x92, 0x60, 0x20, 45);	/* V.17 short */

	rc |= run_switch(0x18, 0x60, 50);	/* V.27ter -> V.29 */
	rc |= run_switch(0x60, 0x91, 51);	/* V.29 -> V.17 */
	rc |= run_switch(0x91, 0x18, 52);	/* V.17 -> V.27ter */

	rc |= run_debug_on();

	return rc;
}

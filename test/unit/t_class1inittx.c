/*
 * t_class1inittx.c -- differential test of `_init_transmitter` (class1tx.c).
 *
 * Same reasoning as t_class1initrx.c: real per-modulation slots (V.17/
 * V.27ter/V.29 transmit), not the NULL ones, because `_init_transmitter`'s
 * own job is to drive `init_vmi_data_tx_modem`/`FAXVMI_create`'s own
 * `vxx_create[slot]`, landing on `V17TX_create`/`V27TX_create`/
 * `V29TX_create` -- already differentially tested elsewhere and routinely
 * invoked as `vxx_create` table entries.
 *
 * UNLIKE THE RECEIVE SIDE, `_init_transmitter` has no `ctx->current_mod`
 * check at all -- an existing modem is ALWAYS torn down and rebuilt, and a
 * SEPARATE, unrelated control-request call (REINIT-flagged) additionally
 * fires whenever the rate code is a V.17 SHORT-TRAINING one, regardless of
 * whether the modem was just freshly built.  Cases below exercise: a bare
 * rebuild for each of the three modulations, a rebuild immediately followed
 * by ANOTHER call (proving the "always rebuild" behaviour rather than a
 * reinit), and the V.17 short-training control-call path specifically.
 */

#include <stddef.h>
#include <string.h>

#include "harness.h"
#include "dsplib/class1.h"
#include "dsplib/class1tx.h"
#include "dsplib/debug.h"
#include "dsplib/faxcfg.h"
#include "dsplib/faxfifo.h"
#include "dsplib/faxvmi.h"

extern void ref__init_transmitter(struct fax_class1 *ctx, int rate_code);
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
	FLD(short_0000);
	FLD(short_0002);
	FLD(int_0004);
	FLD(short_0008);
	FLD(short_000a);
	FLD(short_000c);
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
cmp_ctx(const char *what, struct fax_class1 *a, struct fax_class1 *b,
       long tag)
{
	char buf[64];

	(void)snprintf(buf, sizeof(buf), "%s current_mod (%%ld)", what);
	diff_eq_int(buf, b->current_mod, a->current_mod, tag);
	(void)snprintf(buf, sizeof(buf), "%s state (%%ld)", what);
	diff_eq_int(buf, b->state, a->state, tag);
	(void)snprintf(buf, sizeof(buf), "%s countdown (%%ld)", what);
	diff_eq_int(buf, b->countdown, a->countdown, tag);
	(void)snprintf(buf, sizeof(buf), "%s tx_rate (%%ld)", what);
	diff_eq_int(buf, b->tx_rate, a->tx_rate, tag);
	(void)snprintf(buf, sizeof(buf), "%s f1230 (%%ld)", what);
	diff_eq_int(buf, b->f1230, a->f1230, tag);
	(void)snprintf(buf, sizeof(buf), "%s f1234 (%%ld)", what);
	diff_eq_int(buf, b->f1234, a->f1234, tag);

	cmp_cfg_scalars("modem_vmi", a->modem_vmi, b->modem_vmi, tag);
	cmp_vmi_scalars("vmi_b", a->vmi_b, b->vmi_b, tag);
	cmp_framer("vmi_b framer", a->vmi_b->framer, b->vmi_b->framer, tag);
}

/* One rebuild case: zeroed ctx, `_init_transmitter` called once. */
static int
run_fresh(int rate_code, long tag)
{
	struct fax_class1 ctx_a, ctx_b;

	memset(&ctx_a, 0, sizeof(ctx_a));
	memset(&ctx_b, 0, sizeof(ctx_b));
	ctx_a.clock_sec = ctx_b.clock_sec = 12;
	ctx_a.clock_frac = ctx_b.clock_frac = 34;
	ctx_a.silence_blocks = ctx_b.silence_blocks = 5;

	diff_begin("_init_transmitter: fresh build");

	ref__init_transmitter(&ctx_a, rate_code);
	_init_transmitter(&ctx_b, rate_code);

	cmp_ctx("fresh", &ctx_a, &ctx_b, tag);

	return diff_end();
}

/*
 * Call twice with the SAME rate code -- proves the "always rebuild" shape
 * (no current_mod check): both `modem_vmi`/`vmi_b` end up freshly
 * reallocated the second time, not reused, and every scalar still matches.
 */
static int
run_rebuild(int rate_code, long tag)
{
	struct fax_class1 ctx_a, ctx_b;

	memset(&ctx_a, 0, sizeof(ctx_a));
	memset(&ctx_b, 0, sizeof(ctx_b));
	ctx_a.silence_blocks = ctx_b.silence_blocks = 0;

	ref__init_transmitter(&ctx_a, rate_code);
	_init_transmitter(&ctx_b, rate_code);

	diff_begin("_init_transmitter: unconditional rebuild");

	ref__init_transmitter(&ctx_a, rate_code);
	_init_transmitter(&ctx_b, rate_code);

	cmp_ctx("rebuild", &ctx_a, &ctx_b, tag);

	return diff_end();
}

/* A modulation switch: build V.27ter, then rebuild as V.29, etc. */
static int
run_switch(int rate_code_1, int rate_code_2, long tag)
{
	struct fax_class1 ctx_a, ctx_b;

	memset(&ctx_a, 0, sizeof(ctx_a));
	memset(&ctx_b, 0, sizeof(ctx_b));
	ctx_a.silence_blocks = ctx_b.silence_blocks = 3;

	ref__init_transmitter(&ctx_a, rate_code_1);
	_init_transmitter(&ctx_b, rate_code_1);

	diff_begin("_init_transmitter: modulation switch");

	ref__init_transmitter(&ctx_a, rate_code_2);
	_init_transmitter(&ctx_b, rate_code_2);

	cmp_ctx("switch", &ctx_a, &ctx_b, tag);

	return diff_end();
}

/*
 * V.17 SHORT-TRAINING rate codes (0x4a/0x62/0x7a/0x92): the extra
 * control-request call fires.  Exercised both on a fresh build and
 * immediately after (still fires again, since it is gated on the rate code
 * alone, not on fresh-vs-existing).
 */
static int
run_short_training(int rate_code, long tag)
{
	struct fax_class1 ctx_a, ctx_b;

	memset(&ctx_a, 0, sizeof(ctx_a));
	memset(&ctx_b, 0, sizeof(ctx_b));

	diff_begin("_init_transmitter: V.17 short training");

	ref__init_transmitter(&ctx_a, rate_code);
	_init_transmitter(&ctx_b, rate_code);
	cmp_ctx("short-train fresh", &ctx_a, &ctx_b, tag);

	ref__init_transmitter(&ctx_a, rate_code);
	_init_transmitter(&ctx_b, rate_code);
	cmp_ctx("short-train again", &ctx_a, &ctx_b, tag + 1);

	return diff_end();
}

/*
 * Debug-level coverage, same reasoning as t_class1initrx.c's own
 * `run_debug_on`: proves the debug-gated branches do not diverge.
 */
static int
run_debug_on(void)
{
	struct fax_class1 ctx_a, ctx_b;
	int rc;

	dsplibs_debug_level = ref_dsplibs_debug_level = 2;

	memset(&ctx_a, 0, sizeof(ctx_a));
	memset(&ctx_b, 0, sizeof(ctx_b));

	diff_begin("_init_transmitter: debug_level > 1");

	ref__init_transmitter(&ctx_a, 0x92);	/* V.17 short-training */
	_init_transmitter(&ctx_b, 0x92);
	cmp_ctx("debug fresh v17-short", &ctx_a, &ctx_b, 900);

	ref__init_transmitter(&ctx_a, 0x30);	/* switch to V.27ter */
	_init_transmitter(&ctx_b, 0x30);
	cmp_ctx("debug switch to v27", &ctx_a, &ctx_b, 901);

	rc = diff_end();

	dsplibs_debug_level = ref_dsplibs_debug_level = 0;
	return rc;
}

int
main(void)
{
	int rc = 0;

	rc |= run_fresh(0x18, 10);	/* V.27ter 2400 */
	rc |= run_fresh(0x30, 11);	/* V.27ter 4800 */
	rc |= run_fresh(0x48, 20);	/* V.29 7200 */
	rc |= run_fresh(0x60, 21);	/* V.29 9600 */
	rc |= run_fresh(0x91, 30);	/* V.17 14400 long */
	rc |= run_fresh(0x61, 31);	/* V.17 9600 long */

	rc |= run_rebuild(0x18, 40);
	rc |= run_rebuild(0x60, 41);
	rc |= run_rebuild(0x91, 42);

	rc |= run_switch(0x18, 0x60, 50);	/* V.27ter -> V.29 */
	rc |= run_switch(0x60, 0x91, 51);	/* V.29 -> V.17 */
	rc |= run_switch(0x91, 0x18, 52);	/* V.17 -> V.27ter */

	rc |= run_short_training(0x4a, 60);	/* V.17 7200 short */
	rc |= run_short_training(0x62, 62);	/* V.17 9600 short */
	rc |= run_short_training(0x7a, 64);	/* V.17 12000 short */
	rc |= run_short_training(0x92, 66);	/* V.17 14400 short */

	rc |= run_debug_on();

	return rc;
}

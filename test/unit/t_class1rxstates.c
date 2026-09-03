/*
 * t_class1rxstates.c -- differential tests for `_rx_look_carrier_state`
 * (RX_LOOK_CARRIER, state 12) and `_rx_data_state` (RX_DATA_STATE, state
 * 13).  Both drive `ctx->vmi_b` (and `_rx_look_carrier_state` additionally
 * polls `ctx->vmi_a` when `vmi_b`'s own carrier bit is clear) through
 * `FAXVMI_process`, over the same proven-safe NULL-slot configuration
 * `t_faxvmicp.c`/`t_class1hdlcctl.c` already use.
 *
 * Both sides' VMI objects are separate heap allocations, so the pointer
 * bytes at `vmi_c`/`vmi_a`/`vmi_b` (+0x1200..+0x120b) are patched to match
 * before comparing the rest of the struct -- see t_class1hdlcctl.c's own
 * note on why that is safe here (neither function writes those fields).
 */

#include <stddef.h>
#include <string.h>

#include "harness.h"
#include "dsplib/class1.h"
#include "dsplib/class1tx.h"
#include "dsplib/debug.h"
#include "dsplib/faxcfg.h"
#include "dsplib/faxvmi.h"

extern int ref__rx_look_carrier_state(void *ctx, const short *rx, short *tx,
				      int word3, int word4, int *rx_count,
				      int *tx_count, int word7, int *word8);
extern int ref__rx_data_state(void *ctx, const short *rx, short *tx,
			      int word3, int word4, int *rx_count,
			      int *tx_count, int word7, int *word8);
extern struct faxvmi *ref_FAXVMI_create(struct faxvmi *vmi,
					const struct faxvmi_cfg *cfg);

static const struct faxvmi_cfg NULL_CFG = { 0, 0, 0, 64, 20, 64, 0, NULL,
					     NULL };

#define TX_MAX		200
#define GUARD		16
#define DST_MAX		32

static unsigned long seed = 20260903UL;

static unsigned long
rnd(void)
{
	seed = seed * 1103515245UL + 12345UL;
	return (seed >> 8) & 0xffffffUL;
}

static void
fill(void *p, size_t n)
{
	unsigned char *b = (unsigned char *)p;
	size_t i;

	for (i = 0; i < n; i++)
		b[i] = (unsigned char)(rnd() & 0xff);
}

static struct fax_class1 ctx_a, ctx_b;
static short tx_a[TX_MAX + GUARD], tx_b[TX_MAX + GUARD];
static short rx_a[TX_MAX], rx_b[TX_MAX];
static unsigned char dst_a[DST_MAX], dst_b[DST_MAX];
static int rxc_a, rxc_b, txc_a, txc_b, w8_a, w8_b;

/*
 * `word7` is a plain `int` in the `class1_state_fn` signature, but both
 * functions here treat it as a pointer, `(int *)(long)word7` -- so the
 * argument passed at each call site must be the ADDRESS of one of these,
 * cast to `int`, never a raw value.
 */
static int w7val_a, w7val_b;

static void
patch_vmi_ptrs(void)
{
	memcpy(&ctx_b.vmi_c, &ctx_a.vmi_c,
	       sizeof(ctx_a.vmi_c) + sizeof(ctx_a.vmi_a) + sizeof(ctx_a.vmi_b));
}

/*
 * Each `struct faxvmi` is a FRESH allocation from `FAXVMI_create(NULL, ...)`
 * every call -- passing a non-NULL, not-yet-created pointer would have
 * `FAXVMI_create` dereference that pointer's (garbage) `framer`/`link`
 * fields as already-valid allocations and crash; t_class1hdlcctl.c's own
 * comment records the same fix.  This leaks the previous case's
 * allocations, which is fine for a handful of unit-test cases.
 */
static void
plant(int rxcount, int s7_timeout, int countdown, int word8_in)
{
	struct faxvmi *vmi_b_ref, *vmi_b_ours, *vmi_a_ref, *vmi_a_ours;

	fill(&ctx_a, sizeof(ctx_a));
	memcpy(&ctx_b, &ctx_a, sizeof(ctx_a));

	vmi_b_ref = ref_FAXVMI_create(NULL, &NULL_CFG);
	vmi_b_ours = FAXVMI_create(NULL, &NULL_CFG);
	vmi_a_ref = ref_FAXVMI_create(NULL, &NULL_CFG);
	vmi_a_ours = FAXVMI_create(NULL, &NULL_CFG);

	ctx_a.vmi_c = ctx_a.vmi_b = vmi_b_ref;
	ctx_b.vmi_c = ctx_b.vmi_b = vmi_b_ours;
	ctx_a.vmi_a = vmi_a_ref;
	ctx_b.vmi_a = vmi_a_ours;

	ctx_a.s7_timeout = ctx_b.s7_timeout = s7_timeout;
	ctx_a.countdown = ctx_b.countdown = countdown;
	ctx_a.state = ctx_b.state = CLASS1_RX_LOOK_CARRIER;
	ctx_a.status = ctx_b.status = FAX_CLASS1_NO_MESSAGE;
	ctx_a.delayed_status_countdown = ctx_b.delayed_status_countdown = 0;

	memset(tx_a, (int)(unsigned char)0x5a, sizeof(tx_a));
	memcpy(tx_b, tx_a, sizeof(tx_a));
	fill(rx_a, sizeof(rx_a));
	memcpy(rx_b, rx_a, sizeof(rx_a));
	memset(dst_a, 0xa5, sizeof(dst_a));
	memcpy(dst_b, dst_a, sizeof(dst_a));

	rxc_a = rxc_b = rxcount;
	txc_a = txc_b = (int)0x7e7e7e7e;
	w8_a = w8_b = word8_in;
}

static void
compare_common(long tag)
{
	patch_vmi_ptrs();
	diff_eq_int("ctx (%ld)", memcmp(&ctx_a, &ctx_b, sizeof(ctx_a)), 0,
		    tag);
	diff_eq_int("tx block + guard (%ld)", memcmp(tx_a, tx_b, sizeof(tx_a)),
		    0, tag);
	diff_eq_int("dst block (%ld)", memcmp(dst_a, dst_b, sizeof(dst_a)), 0,
		    tag);
	diff_eq_int("rx_count untouched (%ld)", (long)rxc_b, (long)rxc_a, tag);
	diff_eq_int("tx_count (%ld)", (long)txc_b, (long)txc_a, tag);
	diff_eq_int("word7 target (%ld)", (long)w7val_b, (long)w7val_a, tag);
}

static int
run_look_carrier(void)
{
	static const struct { int rxcount, s7, countdown, w8; } cases[] = {
		{ 160, 100, 0, 0 },
		{ 160, 100, 0, 1 },
		{ 0, 100, 0, 0 },
		{ 160, 100, 100000, 0 },
		{ 160, 0, 100000, 0 },
		{ 160, 5, 1, 0 },
	};
	unsigned i;

	diff_begin("_rx_look_carrier_state");

	for (i = 0; i < sizeof(cases) / sizeof(cases[0]); i++) {
		int ra, rb;
		long tag = (long)i;

		plant(cases[i].rxcount, cases[i].s7, cases[i].countdown,
		      cases[i].w8);
		w7val_a = w7val_b = (int)0x12344321;

		ra = ref__rx_look_carrier_state(&ctx_a, rx_a, tx_a,
		    (int)(long)dst_a, 0, &rxc_a, &txc_a,
		    (int)(long)&w7val_a, &w8_a);
		rb = _rx_look_carrier_state(&ctx_b, rx_b, tx_b,
		    (int)(long)dst_b, 0, &rxc_b, &txc_b,
		    (int)(long)&w7val_b, &w8_b);

		diff_eq_int("return (%ld)", rb, ra, tag);
		compare_common(tag);
		diff_eq_int("word8 (%ld)", (long)w8_b, (long)w8_a, tag);
	}

	return diff_end();
}

/*
 * `_rx_data_state`'s "no carrier" arm (the ELSE of `status &
 * FAXVMI_RESULT_BIT_2000`, which also holds the `*word8 != 0` abort check)
 * is UNREACHABLE through this fixture's null slot: `null_process` always
 * returns -1 (nulldp.h), and the low 24 bits of `FAXVMI_process`'s return
 * are THAT value verbatim (faxvmi.h's own note), so bit 0x2000 is always
 * SET here.  Only the "carrier present" arm is exercised, matching
 * `t_class1hdlcctl.c`'s own precedent for the same shape of gap
 * (`_send_hdlc_buffer_state`'s underrun arm).
 *
 * `w7in` is kept at 0 in every case: a nonzero seed makes `cnt` (and so
 * `FAXVMI_process`'s own `count`) nonzero, which routes RANDOM `fill()`-
 * generated bytes through the async start-bit search `_handle_data_output`
 * runs over `ctx`'s own leading bytes -- exercising that search is
 * `_handle_data_output`'s own test's job, already covered elsewhere; this
 * file's job is only to prove `_rx_data_state` wires the call up correctly,
 * which a count of 0 does without adding search-sensitivity noise.
 */
static int
run_data(void)
{
	static const struct { int rxcount, countdown; } cases[] = {
		{ 160, 0 },
		{ 160, 100 },
		{ 0, 0 },
	};
	unsigned i;

	diff_begin("_rx_data_state");

	for (i = 0; i < sizeof(cases) / sizeof(cases[0]); i++) {
		int ra, rb;
		long tag = (long)i;

		plant(cases[i].rxcount, 100, cases[i].countdown, 0);
		ctx_a.state = ctx_b.state = CLASS1_RX_DATA_STATE;
		w7val_a = w7val_b = 0;

		ra = ref__rx_data_state(&ctx_a, rx_a, tx_a,
		    (int)(long)dst_a, 0, &rxc_a, &txc_a,
		    (int)(long)&w7val_a, &w8_a);
		rb = _rx_data_state(&ctx_b, rx_b, tx_b, (int)(long)dst_b, 0,
		    &rxc_b, &txc_b, (int)(long)&w7val_b, &w8_b);

		diff_eq_int("return (%ld)", rb, ra, tag);
		compare_common(tag);
		diff_eq_int("word8 (%ld)", (long)w8_b, (long)w8_a, tag);
	}

	return diff_end();
}

int
main(void)
{
	int rc = 0;

	rc |= run_look_carrier();
	rc |= run_data();
	return rc;
}

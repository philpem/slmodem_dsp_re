/*
 * t_class1hdlcctl.c -- differential tests for the V.21 HDLC control-channel
 * state handlers: `_send_hdlc_buffer_state` (SEND_HDLC_BUFFER_STATE, state
 * 2), `_send_hdlc_between_buffer_state` (SEND_HDLC_BETWEEN_BUFFER_STATE,
 * state 3), and `cHDLCtx_off` (CLASS1_CHDLCTX_OFF_STATE, state 17).  All
 * three are in `class1tx.c +94`; see that file for each function's own
 * derivation comment.
 *
 * All three call `FAXVMI_process` (and `_send_hdlc_buffer_state` also
 * `FAXVMI_status`) through one of `ctx->vmi_a`/`vmi_c`, so each test case
 * builds a FRESH `struct faxvmi` per side via `FAXVMI_create` over the
 * proven-safe NULL-slot configuration `t_faxvmicp.c` already established
 * (mode SIMP, slot 0, fifo_size 64, max_frame 20, frame_size 64) -- a real
 * per-modulation modem is not needed here and not safely buildable in a
 * unit test.
 *
 * Because the two sides necessarily get two DIFFERENT heap allocations for
 * that `struct faxvmi`, the `vmi_c`/`vmi_a`/`vmi_b` pointer bytes in
 * `ctx_a`/`ctx_b` are patched to match before comparing the rest of the
 * struct -- CLAUDE.md's own allowance ("a loop is still right where some
 * region must be skipped -- two heap pointers hold two different addresses
 * and always will"), applied to exactly those 12 bytes and nothing else.
 * Neither reconstructed function writes any of the three fields (checked
 * against `dis.py`), so this cannot hide a real divergence.
 */

#include <stddef.h>
#include <string.h>

#include "harness.h"
#include "dsplib/class1.h"
#include "dsplib/class1tx.h"
#include "dsplib/debug.h"
#include "dsplib/faxcfg.h"
#include "dsplib/faxvmi.h"

extern int ref__send_hdlc_buffer_state(void *ctx, const short *rx, short *tx,
				       int word3, int word4, int *rx_count,
				       int *tx_count, int word7, int *word8);
extern int ref__send_hdlc_between_buffer_state(void *ctx, const short *rx,
						short *tx, int word3,
						int word4, int *rx_count,
						int *tx_count, int word7,
						int *word8);
extern int ref_cHDLCtx_off(void *ctx, const short *rx, short *tx, int word3,
			   int word4, int *rx_count, int *tx_count, int word7,
			   int *word8);
extern struct faxvmi *ref_FAXVMI_create(struct faxvmi *vmi,
					const struct faxvmi_cfg *cfg);

static const struct faxvmi_cfg NULL_CFG = { 0, 0, 0, 64, 20, 64, 0, NULL,
					     NULL };

#define TX_MAX		160
#define GUARD		16
#define SRC_MAX		12
#define POISON_SH	((short)0x5a5a)

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
static short rx_dummy[8];
static unsigned char src_dummy[SRC_MAX];
static int rxc_a, rxc_b, txc_a, txc_b, w7_a, w7_b, w8_a, w8_b;

/*
 * Overwrite `ctx_b`'s three VMI pointer fields with `ctx_a`'s own values --
 * they are consecutive (`vmi_c`, `vmi_a`, `vmi_b`, +0x1200..+0x120b) so one
 * copy of 3 pointers does all three.  Called ONLY just before a comparison,
 * never before a call: each call still runs against its OWN, separately
 * allocated `struct faxvmi`.
 */
static void
patch_vmi_ptrs(void)
{
	memcpy(&ctx_b.vmi_c, &ctx_a.vmi_c,
	       sizeof(ctx_a.vmi_c) + sizeof(ctx_a.vmi_a) + sizeof(ctx_a.vmi_b));
}

/*
 * Common per-case setup.  A fresh `struct faxvmi` is ALLOCATED (not reused)
 * by `FAXVMI_create(NULL, &NULL_CFG)` on every call, matching t_faxvmicp.c's
 * own proven-safe pattern -- passing a non-NULL, not-yet-created pointer
 * would have `FAXVMI_create` dereference that pointer's (garbage) `framer`/
 * `link` fields as if they were already valid allocations, which is exactly
 * the crash a first version of this fixture had.  So the two sides never
 * carry ring state over from a previous case, and this test leaks the old
 * allocations each `plant()` call -- fine for a handful of unit-test cases.
 */
static void
plant(int countdown, int frame_end_latch, int state, int prev_state, int status)
{
	struct faxvmi *vmi_ref, *vmi_ours;

	fill(&ctx_a, sizeof(ctx_a));
	memcpy(&ctx_b, &ctx_a, sizeof(ctx_a));

	vmi_ref = ref_FAXVMI_create(NULL, &NULL_CFG);
	vmi_ours = FAXVMI_create(NULL, &NULL_CFG);
	ctx_a.vmi_c = ctx_a.vmi_a = vmi_ref;
	ctx_b.vmi_c = ctx_b.vmi_a = vmi_ours;

	ctx_a.countdown = ctx_b.countdown = countdown;
	ctx_a.frame_end_latch = ctx_b.frame_end_latch = frame_end_latch;
	ctx_a.state = ctx_b.state = state;
	ctx_a.prev_state = ctx_b.prev_state = prev_state;
	ctx_a.status = ctx_b.status = status;
	ctx_a.hdlc_write_cursor = ctx_b.hdlc_write_cursor = 1;

	memset(tx_a, (int)(unsigned char)POISON_SH, sizeof(tx_a));
	memcpy(tx_b, tx_a, sizeof(tx_a));
	fill(rx_dummy, sizeof(rx_dummy));
	fill(src_dummy, sizeof(src_dummy));

	/*
	 * DELIBERATELY SMALL, not the usual 0x-repeated-nibble poison this
	 * tree's other tests use: `_send_hdlc_buffer_state` seeds
	 * `FAXVMI_process`'s own `result` parameter from `*tx_count` (and
	 * `cHDLCtx_off` seeds it from `*rx_count`), and that parameter is
	 * NOT purely an output the way its name suggests -- the object's own
	 * `FAXVMI_process`, even with `count` forced to 0, reads the
	 * INCOMING `*result` and corrupts the heap when it is a large,
	 * unrealistic value (confirmed empirically: 0x7e7e7e7e reliably
	 * aborts glibc's `malloc` on the NEXT allocation, in BOTH the
	 * `ref_` and reconstructed builds alike, so this is a property of
	 * the real `FAXVMI_process` contract, not a defect in either side).
	 * A small marker (77/88) is still fully distinguishable from every
	 * real value these three functions write (0, 160, 0x200), so the
	 * "untouched" assertions below lose no coverage.
	 */
	rxc_a = rxc_b = 77;
	txc_a = txc_b = 88;
	w7_a = w7_b = (int)0x12344321;
}

static void
compare_common(long tag)
{
	patch_vmi_ptrs();
	diff_eq_int("ctx (%ld)", memcmp(&ctx_a, &ctx_b, sizeof(ctx_a)), 0,
		    tag);
	diff_eq_int("tx block + guard (%ld)", memcmp(tx_a, tx_b, sizeof(tx_a)),
		    0, tag);
	diff_eq_int("rx_count untouched (%ld)", (long)rxc_b, (long)rxc_a, tag);
	diff_eq_int("word7 (%ld)", (long)w7_b, (long)w7_a, tag);
}

/* -------------------------------------------------------------------- */
/* _send_hdlc_buffer_state                                              */

static int
run_send_hdlc_buffer(void)
{
	static const struct { int countdown; unsigned int debug; } cases[] = {
		{ 0, 0 }, { 0, 2 }, { 5, 0 }, { 5, 2 }, { -1, 0 },
	};
	unsigned i;

	diff_begin("_send_hdlc_buffer_state");

	for (i = 0; i < sizeof(cases) / sizeof(cases[0]); i++) {
		int ra, rb;
		long tag = (long)i;

		plant(cases[i].countdown, 0, CLASS1_SEND_HDLC_BUFFER_STATE, 0,
		      FAX_CLASS1_NO_MESSAGE);
		dsplibs_debug_level = cases[i].debug;
		/*
		 * FAXVMI_process's own `count` local is forced to 0 by this
		 * function before the call, so its return depends only on
		 * `FAXVMI_status`, which the null slot leaves at all-zero
		 * (`underrun` 0) unless something upstream marked it -- so
		 * every one of these cases exercises the "no transition"
		 * arm.  A real underrun cannot be forced without a non-null
		 * modem this test does not build; the transition arm's own
		 * code (state/countdown/`_handle_hdlc_input_open`) is
		 * covered directly by `t_class1handlers.c`'s existing test
		 * of `_handle_hdlc_input_open` and is pure, so nothing here
		 * is untested, only not reached from THIS entry point.
		 */
		w8_a = w8_b = (int)0x87654321;
		txc_a = txc_b = 88;

		ra = ref__send_hdlc_buffer_state(&ctx_a, rx_dummy, tx_a, 0, 0,
		    &rxc_a, &txc_a, w7_a, &w8_a);
		rb = _send_hdlc_buffer_state(&ctx_b, rx_dummy, tx_b, 0, 0,
		    &rxc_b, &txc_b, w7_b, &w8_b);

		diff_eq_int("return (%ld)", rb, ra, tag);
		compare_common(tag);
		diff_eq_int("tx_count untouched (%ld)", (long)txc_b,
			    (long)txc_a, tag);
		diff_eq_int("*word8 == 0x200 (%ld)", (long)w8_a, 0x200, tag);
	}

	dsplibs_debug_level = 0;
	return diff_end();
}

/* -------------------------------------------------------------------- */
/* cHDLCtx_off                                                          */

static int
run_chdlctx_off(void)
{
	static const struct { int countdown; unsigned int debug; } cases[] = {
		{ 0, 0 }, { 1, 0 }, { 2, 0 }, { 3, 0 }, { 3, 2 },
		{ 14, 0 }, { 15, 0 }, { 16, 0 }, { 16, 2 }, { -1, 0 },
	};
	unsigned i;

	diff_begin("cHDLCtx_off");

	for (i = 0; i < sizeof(cases) / sizeof(cases[0]); i++) {
		int ra, rb;
		long tag = (long)i;

		plant(cases[i].countdown, 0, CLASS1_CHDLCTX_OFF_STATE, 0,
		      FAX_CLASS1_NO_MESSAGE);
		dsplibs_debug_level = cases[i].debug;
		rxc_a = rxc_b = 4;
		txc_a = txc_b = 88;
		w8_a = w8_b = (int)0x87654321;

		ra = ref_cHDLCtx_off(&ctx_a, rx_dummy, tx_a, 0, 0, &rxc_a,
		    &txc_a, w7_a, &w8_a);
		rb = cHDLCtx_off(&ctx_b, rx_dummy, tx_b, 0, 0, &rxc_b, &txc_b,
		    w7_b, &w8_b);

		diff_eq_int("return (%ld)", rb, ra, tag);
		compare_common(tag);
		diff_eq_int("tx_count == block (%ld)", (long)txc_a,
			    CLASS1_BLOCK_SAMPLES, tag);
		diff_eq_int("tx_count matches (%ld)", (long)txc_b,
			    (long)txc_a, tag);
		diff_eq_int("word8 untouched (%ld)", (long)w8_b, (long)w8_a,
			    tag);
	}

	dsplibs_debug_level = 0;
	return diff_end();
}

/* -------------------------------------------------------------------- */
/* _send_hdlc_between_buffer_state                                      */

static int
run_send_hdlc_between_buffer(void)
{
	static const struct {
		int countdown, frame_end_latch, word8_in;
		unsigned int debug;
	} cases[] = {
		/* countdown != 2: latch untouched either way */
		{ 0, 0, 0, 0 },
		{ 0, 0, -1, 0 },
		{ 1, 1, -1, 0 },
		/* countdown == 2, latch: CONNECT vs idle-transition */
		{ 2, 0, 0, 0 },
		{ 2, 1, 0, 0 },
		{ 2, 1, 0, 2 },
		/* countdown == 2, and word8 > 0 so the read branch also runs */
		{ 2, 0, 1, 0 },
		{ 2, 1, 1, 0 },
		/* countdown > 250 threshold */
		{ 251, 0, 0, 0 },
		{ 251, 0, 0, 2 },
		{ 249, 0, 0, 0 },
	};
	unsigned i;

	diff_begin("_send_hdlc_between_buffer_state");

	for (i = 0; i < sizeof(cases) / sizeof(cases[0]); i++) {
		int ra, rb;
		long tag = (long)i;

		plant(cases[i].countdown, cases[i].frame_end_latch,
		      CLASS1_SEND_HDLC_BETWEEN_BUFFER_STATE, 0,
		      FAX_CLASS1_NO_MESSAGE);
		dsplibs_debug_level = cases[i].debug;

		w8_a = w8_b = cases[i].word8_in;
		txc_a = txc_b = 88;

		ra = ref__send_hdlc_between_buffer_state(&ctx_a, rx_dummy,
		    tx_a, 0, (int)(long)src_dummy, &rxc_a, &txc_a, w7_a,
		    &w8_a);
		rb = _send_hdlc_between_buffer_state(&ctx_b, rx_dummy, tx_b,
		    0, (int)(long)src_dummy, &rxc_b, &txc_b, w7_b, &w8_b);

		diff_eq_int("return (%ld)", rb, ra, tag);
		compare_common(tag);
		diff_eq_int("tx_count == block (%ld)", (long)txc_a,
			    CLASS1_BLOCK_SAMPLES, tag);
		diff_eq_int("tx_count matches (%ld)", (long)txc_b,
			    (long)txc_a, tag);
		diff_eq_int("*word8 == 0x200 (%ld)", (long)w8_a, 0x200, tag);
	}

	dsplibs_debug_level = 0;
	return diff_end();
}

int
main(void)
{
	int rc = 0;

	rc |= run_send_hdlc_buffer();
	rc |= run_chdlctx_off();
	rc |= run_send_hdlc_between_buffer();
	return rc;
}

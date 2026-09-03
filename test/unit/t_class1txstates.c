/*
 * t_class1txstates.c -- differential tests for eight of the nine remaining
 * `class1tx.c +94` state handlers this wave lands: `_rx_look_carrier_state`
 * (12), `_rx_data_state` (13), `_tx_nulls_state` (11), `_tx_scrambled_ones_
 * state` (9), `_tx_data_state` (10), `_hdlc_receive_state` (5), `_hdlc_
 * receive_between_buffers_state` (6), `_hdlc_receive_look_carrier_state`
 * (4) and `_t30_preabmle_state` (1).
 * `t_class1hdlcctl.c` covers three more (`_send_hdlc_buffer_state`,
 * `_send_hdlc_between_buffer_state`, `cHDLCtx_off`) landed this same wave.
 *
 * VMI SETUP.  Every function here drives `ctx->vmi_a`, `ctx->vmi_b` or both
 * through `FAXVMI_process`.  `t_class1hdlcctl.c`'s own NULL-slot config
 * (mode SIMP, slot 0 -> `null_process`, which always returns -1) is reused
 * here for the same reason it works there: it is the only slot this tree
 * can safely build in a unit test without a real modulation's own training
 * sequence.  `null_process` returning -1 means `FAXVMI_RESULT_BIT_2000` is
 * ALWAYS set on the null slot -- every "carrier/bit clear" branch in the
 * functions below is therefore NOT reached from these tests and is noted as
 * such per-function, the same allowance `t_class1hdlcctl.c`'s own
 * `run_send_hdlc_buffer` comment already uses ("not reached from THIS entry
 * point"). `_hdlc_receive_state` also chases `ctx->vmi_a->link->int_0014`'s
 * own +0x50 -- ONLY on the path where the unpack step actually produced a
 * nonzero element count, which `null_process`'s no-op pack/unpack never
 * does, so that chase is provably never dereferenced from this file's own
 * `_hdlc_receive_state` cases; a true structural fact checked against
 * `dis.py`, not an assumption. `_hdlc_receive_look_carrier_state` makes the
 * SAME chase unconditionally instead, so its own test below overrides
 * `link->int_0014` with a synthetic fixture rather than relying on the null
 * slot -- see that test's own comment.
 *
 * FIFO SETUP.  `_tx_nulls_state`, `_tx_scrambled_ones_state` and
 * `_tx_data_state` all drive `ctx->f1288` (a `struct fax_fifo *`) and read
 * `ctx->f1290` (the per-call block size `FIFO_read`/`FIFO_write` are asked
 * for).  A real `FIFO_create(NULL, NULL)` (the object's own default,
 * `FIFO_CFG` = {0, 100, 0}) gives a 100-element ring, comfortably above the
 * small `f1290` values these tests use, on BOTH sides independently (`ours`
 * and the blob's own `ref_FIFO_create`) -- `t_class1delmodem.c` already
 * establishes this pattern.
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

extern int ref__rx_look_carrier_state(void *ctx, const short *rx, short *tx,
				      int word3, int word4, int *rx_count,
				      int *tx_count, int word7, int *word8);
extern int ref__rx_data_state(void *ctx, const short *rx, short *tx,
			      int word3, int word4, int *rx_count,
			      int *tx_count, int word7, int *word8);
extern int ref__tx_nulls_state(void *ctx, const short *rx, short *tx,
			       int word3, int word4, int *rx_count,
			       int *tx_count, int word7, int *word8);
extern int ref__tx_scrambled_ones_state(void *ctx, const short *rx,
					short *tx, int word3, int word4,
					int *rx_count, int *tx_count,
					int word7, int *word8);
extern int ref__tx_data_state(void *ctx, const short *rx, short *tx,
			      int word3, int word4, int *rx_count,
			      int *tx_count, int word7, int *word8);
extern int ref__hdlc_receive_state(void *ctx, const short *rx, short *tx,
				   int word3, int word4, int *rx_count,
				   int *tx_count, int word7, int *word8);
extern int ref__hdlc_receive_between_buffers_state(void *ctx,
						    const short *rx,
						    short *tx, int word3,
						    int word4, int *rx_count,
						    int *tx_count, int word7,
						    int *word8);
extern int ref__hdlc_receive_look_carrier_state(void *ctx, const short *rx,
						 short *tx, int word3,
						 int word4, int *rx_count,
						 int *tx_count, int word7,
						 int *word8);
extern int ref__t30_preabmle_state(void *ctx, const short *rx, short *tx,
				   int word3, int word4, int *rx_count,
				   int *tx_count, int word7, int *word8);
extern struct faxvmi *ref_FAXVMI_create(struct faxvmi *vmi,
					const struct faxvmi_cfg *cfg);
extern struct fax_fifo *ref_FIFO_create(struct fax_fifo *f,
					const struct fifo_cfg *cfg);

static const struct faxvmi_cfg NULL_CFG = { 0, 0, 0, 64, 20, 64, 0, NULL,
					     NULL };

#define TX_MAX		200
#define GUARD		16
#define SRC_MAX		16

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
static short rx_a[TX_MAX + GUARD], rx_b[TX_MAX + GUARD];
static unsigned char src_a[SRC_MAX], src_b[SRC_MAX];
static int rxc_a, rxc_b, txc_a, txc_b, w7_a, w7_b, w8_a, w8_b;

/*
 * `vmi_c`/`vmi_a`/`vmi_b` (+0x1200..+0x120b) AND `f1288` (+0x1288, a
 * `struct fax_fifo *`) are the only heap pointers a `plant()`-built pair
 * ever disagrees on -- both sides allocate their OWN, separately.
 */
static void
patch_heap_ptrs(void)
{
	memcpy(&ctx_b.vmi_c, &ctx_a.vmi_c,
	       sizeof(ctx_a.vmi_c) + sizeof(ctx_a.vmi_a) + sizeof(ctx_a.vmi_b));
	ctx_b.f1288 = ctx_a.f1288;
}

/*
 * `src_dummy` doubles as `word4`'s target (cast through `(long)`, the same
 * idiom every function below uses) -- a 16-element buffer is enough for
 * every `_handle_data_input`/`_handle_hdlc_input` call these tests drive,
 * since none asks for more than a handful of bytes.
 *
 * `FAXVMI_create`/`FIFO_create` are called with a NULL first argument, so
 * EACH allocates its own fresh object rather than re-initialising
 * caller-provided storage -- `t_class1hdlcctl.c`'s own comment on `plant()`
 * records why the other order (passing the address of a not-yet-created,
 * garbage-filled local) crashes: `FAXVMI_create`'s "reinit in place" arm
 * dereferences that garbage as if it were an existing allocation's
 * `framer`/`link` fields.  This leaks the old allocation each call, which is
 * fine for a handful of unit-test cases.
 */
static void
plant(int state, int prev_state, int with_fifo)
{
	fill(&ctx_a, sizeof(ctx_a));
	memcpy(&ctx_b, &ctx_a, sizeof(ctx_a));

	ctx_a.vmi_c = ctx_a.vmi_a = ctx_a.vmi_b =
	    ref_FAXVMI_create(NULL, &NULL_CFG);
	ctx_b.vmi_c = ctx_b.vmi_a = ctx_b.vmi_b =
	    FAXVMI_create(NULL, &NULL_CFG);

	ctx_a.f1288 = with_fifo ? ref_FIFO_create(NULL, NULL) : NULL;
	ctx_b.f1288 = with_fifo ? FIFO_create(NULL, NULL) : NULL;

	ctx_a.state = ctx_b.state = state;
	ctx_a.prev_state = ctx_b.prev_state = prev_state;
	ctx_a.status = ctx_b.status = FAX_CLASS1_NO_MESSAGE;
	ctx_a.countdown = ctx_b.countdown = 0;
	ctx_a.f1224 = ctx_b.f1224 = 0;
	ctx_a.f1250 = ctx_b.f1250 = 1;
	ctx_a.f12d0 = ctx_b.f12d0 = 0;
	ctx_a.s7_timeout = ctx_b.s7_timeout = 60;
	ctx_a.cng_enabled = ctx_b.cng_enabled = 0;
	ctx_a.f125c = ctx_b.f125c = 0;
	ctx_a.f1260 = ctx_b.f1260 = 0;
	ctx_a.transmit_enabled = ctx_b.transmit_enabled = 0;
	ctx_a.f1270 = ctx_b.f1270 = 0;
	ctx_a.f1290 = ctx_b.f1290 = 10;
	ctx_a.f1294 = ctx_b.f1294 = 0;
	ctx_a.f1298 = ctx_b.f1298 = 0;
	ctx_a.hdlc_frame_done = ctx_b.hdlc_frame_done = 0;
	ctx_a.buffers_sent = ctx_b.buffers_sent = 0;
	ctx_a.last_in_byte = ctx_b.last_in_byte = 0;
	/*
	 * `_handle_data_input`'s FIRST check is `data_input_closed != 0`,
	 * which short-circuits it entirely -- left to `fill()`'s random
	 * bytes, that field is nonzero on all but a vanishing fraction of
	 * runs, so the TX trio's own `_handle_data_input` call would go
	 * almost untested by chance rather than by design.  Cleared here so
	 * the interesting path (`*word8 > 0`) is genuinely exercised.
	 */
	ctx_a.data_input_closed = ctx_b.data_input_closed = 0;
	ctx_a.dle_seen = ctx_b.dle_seen = 0;

	memset(tx_a, 0x5a, sizeof(tx_a));
	memcpy(tx_b, tx_a, sizeof(tx_a));
	fill(rx_a, sizeof(rx_a));
	memcpy(rx_b, rx_a, sizeof(rx_a));
	fill(src_a, sizeof(src_a));
	memcpy(src_b, src_a, sizeof(src_a));

	w7_a = w7_b = (int)0x12344321;
}

static void
compare_common(long tag)
{
	patch_heap_ptrs();
	diff_eq_obj("ctx", struct fax_class1, &ctx_b, &ctx_a, tag);
	diff_eq_int("tx block + guard (%ld)", memcmp(tx_a, tx_b, sizeof(tx_a)),
		    0, tag);
	diff_eq_int("word7 (%ld)", (long)w7_b, (long)w7_a, tag);
}

/* -------------------------------------------------------------------- */
/* _rx_look_carrier_state / _rx_data_state                              */

static int
run_rx_pair(void)
{
	static const int rxcounts[] = { 160, 0, 40 };
	unsigned i;
	int rc = 0;

	diff_begin("_rx_look_carrier_state");
	for (i = 0; i < sizeof(rxcounts) / sizeof(rxcounts[0]); i++) {
		int ra, rb;
		long tag = (long)i;

		plant(CLASS1_RX_LOOK_CARRIER, 0, 0);
		rxc_a = rxc_b = rxcounts[i];
		txc_a = txc_b = (int)0x7e7e7e7e;
		w8_a = w8_b = 0;

		ra = ref__rx_look_carrier_state(&ctx_a, rx_a, tx_a, 0, 0,
		    &rxc_a, &txc_a, w7_a, &w8_a);
		rb = _rx_look_carrier_state(&ctx_b, rx_b, tx_b, 0, 0, &rxc_b,
		    &txc_b, w7_b, &w8_b);

		diff_eq_int("return (%ld)", rb, ra, tag);
		compare_common(tag);
		diff_eq_int("rx_count untouched (%ld)", (long)rxc_b,
			    (long)rxc_a, tag);
		diff_eq_int("tx_count == rx_count (%ld)", (long)txc_b,
			    (long)txc_a, tag);
		diff_eq_int("word8 == 5 (%ld)", (long)w8_a, 5, tag);
	}
	rc |= diff_end();

	diff_begin("_rx_data_state");
	for (i = 0; i < sizeof(rxcounts) / sizeof(rxcounts[0]); i++) {
		int ra, rb;
		long tag = (long)i;
		/*
		 * `word7` is a plain `int` in the `class1_state_fn`
		 * signature, but `_rx_data_state` treats it as a pointer,
		 * `(int *)(long)word7` -- both dereferenced at entry (to
		 * seed `cnt`) and written at exit.  It must be the ADDRESS
		 * of a real variable, cast to `int`, never a raw value: a
		 * first version of this loop passed the literal `3`, which
		 * dereferences address 0x3 and segfaults immediately.  Kept
		 * at 0 (not e.g. 3) to sidestep `_handle_data_output`'s own
		 * async-search sensitivity to a nonzero seed over `fill()`-
		 * random `ctx` bytes -- see `t_class1rxstates.c`'s own note
		 * on the same tradeoff.
		 */
		int w7val_a = 0, w7val_b = 0;

		plant(CLASS1_RX_DATA_STATE, 0, 0);
		rxc_a = rxc_b = rxcounts[i];
		txc_a = txc_b = (int)0x7e7e7e7e;
		w8_a = w8_b = 0;

		ra = ref__rx_data_state(&ctx_a, rx_a, tx_a, (int)(long)src_a,
		    0, &rxc_a, &txc_a, (int)(long)&w7val_a, &w8_a);
		rb = _rx_data_state(&ctx_b, rx_b, tx_b, (int)(long)src_b, 0,
		    &rxc_b, &txc_b, (int)(long)&w7val_b, &w8_b);

		diff_eq_int("return (%ld)", rb, ra, tag);
		compare_common(tag);
		diff_eq_int("word8 == 5 (%ld)", (long)w8_a, 5, tag);
	}
	rc |= diff_end();

	return rc;
}

/* -------------------------------------------------------------------- */
/* _tx_nulls_state                                                       */

static int
run_tx_nulls(void)
{
	static const int word8s[] = { 0, 4, -1 };
	unsigned i;

	diff_begin("_tx_nulls_state");
	for (i = 0; i < sizeof(word8s) / sizeof(word8s[0]); i++) {
		int ra, rb;
		long tag = (long)i;

		plant(CLASS1_TX_NULLS_STATE, 0, 1);
		w8_a = w8_b = word8s[i];
		/*
		 * SMALL, not the usual repeated-nibble poison: this function
		 * seeds `FAXVMI_process`'s own `result` parameter from
		 * `*tx_count`, and that parameter is not purely an output --
		 * the object's own `FAXVMI_process`, even with `count` forced
		 * to 0, reads the INCOMING `*result` and corrupts the heap
		 * when it is unrealistically large (confirmed empirically:
		 * 0x7e7e7e7e reliably aborts glibc's `malloc` on the NEXT
		 * allocation, in the `ref_` build and the reconstructed one
		 * alike, so this is a property of the real contract, not a
		 * defect in either side).  See `t_class1hdlcctl.c`'s own
		 * `plant()` for the first place this was found.
		 */
		txc_a = txc_b = 88;

		/*
		 * word4, not word3, is _handle_data_input's src -- a first
		 * version of every call in this file had these swapped,
		 * which happened to not crash here only because `ctx->
		 * data_input_closed` (random `fill()` bytes) is nonzero
		 * almost always, short-circuiting `_handle_data_input`
		 * before it would dereference the NULL `word4` this left
		 * behind.
		 */
		ra = ref__tx_nulls_state(&ctx_a, rx_a, tx_a, 0,
		    (int)(long)src_a, &rxc_a, &txc_a, w7_a, &w8_a);
		rb = _tx_nulls_state(&ctx_b, rx_b, tx_b, 0, (int)(long)src_b,
		    &rxc_b, &txc_b, w7_b, &w8_b);

		diff_eq_int("return (%ld)", rb, ra, tag);
		compare_common(tag);
		diff_eq_int("word8 (%ld)", (long)w8_b, (long)w8_a, tag);
	}
	return diff_end();
}

/* -------------------------------------------------------------------- */
/* _tx_scrambled_ones_state                                              */

static int
run_tx_scrambled_ones(void)
{
	static const struct { int f1270, word8; } cases[] = {
		{ 0, 0 }, { 1, 0 }, { 0, 4 }, { 1, 4 },
	};
	unsigned i;

	diff_begin("_tx_scrambled_ones_state");
	for (i = 0; i < sizeof(cases) / sizeof(cases[0]); i++) {
		int ra, rb;
		long tag = (long)i;

		plant(CLASS1_TX_SCRAMBLED_ONES_STATE, 0, 1);
		ctx_a.f1270 = ctx_b.f1270 = cases[i].f1270;
		w8_a = w8_b = cases[i].word8;
		txc_a = txc_b = 88; /* see run_tx_nulls's own note */

		/* word4, not word3, is _handle_data_input's src (see
		 * run_tx_nulls's own note on the same fix). */
		ra = ref__tx_scrambled_ones_state(&ctx_a, rx_a, tx_a, 0,
		    (int)(long)src_a, &rxc_a, &txc_a, w7_a, &w8_a);
		rb = _tx_scrambled_ones_state(&ctx_b, rx_b, tx_b, 0,
		    (int)(long)src_b, &rxc_b, &txc_b, w7_b, &w8_b);

		diff_eq_int("return (%ld)", rb, ra, tag);
		compare_common(tag);
		diff_eq_int("word8 (%ld)", (long)w8_b, (long)w8_a, tag);
	}
	return diff_end();
}

/* -------------------------------------------------------------------- */
/* _tx_data_state                                                        */

static int
run_tx_data(void)
{
	static const struct { int last_in_byte, word8; } cases[] = {
		{ 0, 0 }, { 1, 0 }, { 0, 4 }, { 1, 4 },
	};
	unsigned i;

	diff_begin("_tx_data_state");
	for (i = 0; i < sizeof(cases) / sizeof(cases[0]); i++) {
		int ra, rb;
		long tag = (long)i;

		plant(CLASS1_TX_DATA_STATE, 0, 1);
		ctx_a.last_in_byte = ctx_b.last_in_byte =
		    cases[i].last_in_byte;
		w8_a = w8_b = cases[i].word8;
		txc_a = txc_b = 88; /* see run_tx_nulls's own note */

		/* word4, not word3, is _handle_data_input's src (see
		 * run_tx_nulls's own note on the same fix). */
		ra = ref__tx_data_state(&ctx_a, rx_a, tx_a, 0,
		    (int)(long)src_a, &rxc_a, &txc_a, w7_a, &w8_a);
		rb = _tx_data_state(&ctx_b, rx_b, tx_b, 0, (int)(long)src_b,
		    &rxc_b, &txc_b, w7_b, &w8_b);

		diff_eq_int("return (%ld)", rb, ra, tag);
		compare_common(tag);
		diff_eq_int("word8 (%ld)", (long)w8_b, (long)w8_a, tag);
	}
	return diff_end();
}

/* -------------------------------------------------------------------- */
/* _hdlc_receive_state / _hdlc_receive_between_buffers_state             */
/*                                                                        */
/* Both drive `ctx->vmi_a` and, per this file's own top comment, never    */
/* dereference the `link->int_0014`/+0x50 chase from the NULL slot -- the */
/* unpack step's own `count` stays 0, so `cnt != 0` is never reached.     */

static int
run_hdlc_receive_pair(void)
{
	static const int word8s[] = { 0, 1 };
	static const int prev_states[] = { 0,
		CLASS1_HDLC_RECEIVE_BETWEEN_BUFFERS_STATE };
	unsigned i, j;
	int rc = 0;

	diff_begin("_hdlc_receive_state");
	for (i = 0; i < sizeof(prev_states) / sizeof(prev_states[0]); i++) {
		for (j = 0; j < sizeof(word8s) / sizeof(word8s[0]); j++) {
			int ra, rb;
			long tag = (long)(i * 10 + j);
			/*
			 * `word7` is a pointer here too (`*(int *)(long)
			 * word7`, written on the no-carrier AND the
			 * count/f000-nonzero arms) -- needs real backing
			 * storage, the same fix `run_rx_pair`'s own
			 * `_rx_data_state` loop needed.
			 */
			int w7val_a = 0, w7val_b = 0;

			plant(CLASS1_HDLC_RECEIVE_STATE, prev_states[i], 0);
			/*
			 * These two seed `FAXVMI_process`'s `result`
			 * (rxc, per `_hdlc_receive_state`'s own `unsigned
			 * short result = (unsigned short)*rx_count;`) --
			 * kept small for the same reason `run_tx_nulls`'s
			 * own note gives.
			 */
			rxc_a = rxc_b = 160;
			txc_a = txc_b = (int)0x7e7e7e7e;
			w8_a = w8_b = word8s[j];

			ra = ref__hdlc_receive_state(&ctx_a, rx_a, tx_a,
			    (int)(long)src_a, 0, &rxc_a, &txc_a,
			    (int)(long)&w7val_a, &w8_a);
			rb = _hdlc_receive_state(&ctx_b, rx_b, tx_b,
			    (int)(long)src_b, 0, &rxc_b, &txc_b,
			    (int)(long)&w7val_b, &w8_b);

			diff_eq_int("return (%ld)", rb, ra, tag);
			compare_common(tag);
			diff_eq_int("word8 == 5 (%ld)", (long)w8_a, 5, tag);
			diff_eq_int("tx_count == block (%ld)", (long)txc_a,
				    CLASS1_BLOCK_SAMPLES, tag);
		}
	}
	rc |= diff_end();

	diff_begin("_hdlc_receive_between_buffers_state");
	for (j = 0; j < sizeof(word8s) / sizeof(word8s[0]); j++) {
		int ra, rb;
		long tag = (long)j;
		/* word7 is a pointer here too, on the *word8 != 0 arm. */
		int w7val_a = 0, w7val_b = 0;

		plant(CLASS1_HDLC_RECEIVE_BETWEEN_BUFFERS_STATE, 0, 0);
		rxc_a = rxc_b = 160;
		txc_a = txc_b = (int)0x7e7e7e7e;
		w8_a = w8_b = word8s[j];

		ra = ref__hdlc_receive_between_buffers_state(&ctx_a, rx_a,
		    tx_a, (int)(long)src_a, 0, &rxc_a, &txc_a,
		    (int)(long)&w7val_a, &w8_a);
		rb = _hdlc_receive_between_buffers_state(&ctx_b, rx_b, tx_b,
		    (int)(long)src_b, 0, &rxc_b, &txc_b,
		    (int)(long)&w7val_b, &w8_b);

		diff_eq_int("return (%ld)", rb, ra, tag);
		compare_common(tag);
		diff_eq_int("word8 == 5 (%ld)", (long)w8_a, 5, tag);
	}
	rc |= diff_end();

	return rc;
}

/* -------------------------------------------------------------------- */
/* _hdlc_receive_look_carrier_state                                      */
/*                                                                        */
/* Same NULL-slot argument as the pair above: `FAXVMI_RESULT_BIT_2000` is */
/* always SET (null_process returns -1), which sends every case through   */
/* the table-scan arm -- and THIS function's own chase of `vmi_a->link->  */
/* int_0014`'s own +0x50 is UNCONDITIONAL there (unlike the two functions  */
/* above, where the same chase is behind `cnt != 0`), so it cannot be      */
/* left at 0: `null_create` never sets `link->int_0014` (it zeroes it),    */
/* and reading `*(char **)(0 + 0x50)` is a null-pointer fault.  This does  */
/* NOT need a real V.21RX slot, though -- the chase is raw offset          */
/* arithmetic through `int_0014`, `+0x50` and `+0x2c` with no other field  */
/* of `link` or of what it points at ever read, so a synthetic fixture at  */
/* those three offsets is exactly as safe as a real modem's own, and both  */
/* `ctx_a`/`ctx_b` read the SAME fixture bytes since this is one process:  */
/* there is nothing for the two sides to disagree on that the object's own */
/* logic does not already make identical.  `lc_link_target.chase` supplies */
/* the +0x50 pointer and `lc_quality_target.level` the value compared      */
/* against `HDLC_LOOK_CARRIER_LEVELS[i]` (514/727/1026/1450, `.rodata`     */
/* 0xba22) -- three cases below hit i==0, i==2 and no match at all.        */
static struct {
	unsigned char pad[0x50];
	void *chase;
} lc_link_target;
static struct {
	unsigned char pad[0x2c];
	short level;
} lc_quality_target;

static int
run_hdlc_receive_look_carrier(void)
{
	static const short levels[] = { 100, 800, 2000 };
	unsigned i;

	diff_begin("_hdlc_receive_look_carrier_state");
	for (i = 0; i < sizeof(levels) / sizeof(levels[0]); i++) {
		int ra, rb;
		long tag = (long)i;

		plant(CLASS1_HDLC_RECEIVE_LOOK_CARRIER_STATE, 0, 0);
		rxc_a = rxc_b = 160;
		txc_a = txc_b = (int)0x7e7e7e7e;
		w8_a = w8_b = 0;

		lc_quality_target.level = levels[i];
		lc_link_target.chase = &lc_quality_target;
		ctx_a.vmi_a->link->int_0014 = (int)(long)&lc_link_target;
		ctx_b.vmi_a->link->int_0014 = (int)(long)&lc_link_target;

		ra = ref__hdlc_receive_look_carrier_state(&ctx_a, rx_a, tx_a,
		    0, 0, &rxc_a, &txc_a, w7_a, &w8_a);
		rb = _hdlc_receive_look_carrier_state(&ctx_b, rx_b, tx_b, 0,
		    0, &rxc_b, &txc_b, w7_b, &w8_b);

		diff_eq_int("return (%ld)", rb, ra, tag);
		compare_common(tag);
		diff_eq_int("word8 == 5 (%ld)", (long)w8_a, 5, tag);
	}
	return diff_end();
}

/* -------------------------------------------------------------------- */
/* _t30_preabmle_state                                                   */

static int
run_t30_preabmle(void)
{
	static const int word8s[] = { 0, 4 };
	unsigned i;

	diff_begin("_t30_preabmle_state");
	for (i = 0; i < sizeof(word8s) / sizeof(word8s[0]); i++) {
		int ra, rb;
		long tag = (long)i;

		plant(CLASS1_T30_PREAMBLE_STATE, 0, 0);
		w8_a = w8_b = word8s[i];
		/*
		 * `_t30_preabmle_state` seeds `result` as a plain literal 0,
		 * never from `*tx_count` -- unlike the TX trio above, so this
		 * poison value is not under the same constraint.  Kept as
		 * `0x7e7e7e7e` (this file's original choice) since it is
		 * genuinely safe here and lets the "never written" checks
		 * stay maximally distinctive.
		 */
		txc_a = txc_b = (int)0x7e7e7e7e;

		/* word4, not word3, is _handle_hdlc_input's src here too
		 * (see run_tx_nulls's own note on the same fix). */
		ra = ref__t30_preabmle_state(&ctx_a, rx_a, tx_a, 0,
		    (int)(long)src_a, &rxc_a, &txc_a, w7_a, &w8_a);
		rb = _t30_preabmle_state(&ctx_b, rx_b, tx_b, 0,
		    (int)(long)src_b, &rxc_b, &txc_b, w7_b, &w8_b);

		diff_eq_int("return (%ld)", rb, ra, tag);
		compare_common(tag);
		diff_eq_int("word8 == 0x200 (%ld)", (long)w8_a, 0x200, tag);
		diff_eq_int("tx_count == block (%ld)", (long)txc_a,
			    CLASS1_BLOCK_SAMPLES, tag);
	}
	return diff_end();
}

int
main(void)
{
	int rc = 0;

	rc |= run_rx_pair();
	rc |= run_tx_nulls();
	rc |= run_tx_scrambled_ones();
	rc |= run_tx_data();
	rc |= run_hdlc_receive_pair();
	rc |= run_hdlc_receive_look_carrier();
	rc |= run_t30_preabmle();
	return rc;
}

/*
 * t_class1hdlcemu.c -- differential test of `_hdlc_emulate_receive_state`.
 *
 * `.text` 0x09e1b0, 452 bytes -- HDLC_EMULATE_RECEIVE_STATE, state 7.  It
 * replays length-prefixed records out of `ctx->superframe` (class1.h) one per
 * call, gated by the between-record countdown `superframe_countdown`.  The fixture builds a
 * VALID run of records in `superframe` -- never more than `CLASS1_EMU_MAX_FRAMES`
 * of them, matching the object's own unguarded stack table (class1.h's own
 * comment on that constant) -- and sweeps `superframe_read_idx` (which record is next),
 * `superframe_countdown` (whether the countdown has expired) and `prev_state` (first tick
 * into this state or a continuation) across every combination the function's
 * control flow distinguishes.
 *
 * COVERAGE IS ASSERTED FROM THE RUN (F134), via what the REFERENCE actually
 * left in `ctx_a` after each case: the CONNECT status write, the
 * NO_CARRIER_NO_MESSAGE/IDLE transition, an emitted record, and the
 * buffer-exhausted reset are each counted and asserted nonzero.
 */

#include <stddef.h>
#include <string.h>

#include "harness.h"
#include "dsplib/class1.h"
#include "dsplib/class1tx.h"

extern int ref__hdlc_emulate_receive_state(void *ctx, const short *rx,
					   short *tx, int word3, int word4,
					   int *rx_count, int *tx_count,
					   int word7, int *word8);

#define TX_MAX		160
#define GUARD		16
#define DST_MAX		64
#define POISON_SH	((short)0x5a5a)
#define POISON_CH	((unsigned char)0xa5)

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
static unsigned char dst_a[DST_MAX + GUARD], dst_b[DST_MAX + GUARD];
static short rx_dummy[8];
static int rxc_a, rxc_b, txc_a, txc_b, w7_a, w7_b, w8_a, w8_b;

/*
 * Build `nrec` valid records into `ctx->superframe`: a random length of 0..5
 * bytes, then that many random content bytes.  Returns the total byte count
 * used -- the value `superframe_len` must be set to for the parse loop to find
 * exactly `nrec` records.
 */
static int
build_records(struct fax_class1 *ctx, int nrec)
{
	int idx = 0;
	int r;

	for (r = 0; r < nrec; r++) {
		int len = (int)(rnd() % 6);
		int k;

		ctx->superframe[idx] = (unsigned short)len;
		for (k = 0; k < len; k++)
			ctx->superframe[idx + 1 + k] =
			    (unsigned short)(rnd() & 0xff);
		idx += len + 1;
	}
	return idx;
}

static void
plant(int nrec, int next, int old_f12c8, int prev_state)
{
	int total;

	fill(&ctx_a, sizeof(ctx_a));
	total = build_records(&ctx_a, nrec);
	memcpy(&ctx_b, &ctx_a, sizeof(ctx_a));

	ctx_a.superframe_len = ctx_b.superframe_len = total;
	ctx_a.superframe_read_idx = ctx_b.superframe_read_idx = next;
	ctx_a.superframe_countdown = ctx_b.superframe_countdown = old_f12c8;
	ctx_a.prev_state = ctx_b.prev_state = prev_state;
	ctx_a.state = ctx_b.state = CLASS1_HDLC_EMULATE_RECEIVE_STATE;
	ctx_a.status = ctx_b.status = FAX_CLASS1_NO_MESSAGE;
	ctx_a.delayed_status = ctx_b.delayed_status = (int)0x7e7e7e7e;
	ctx_a.delayed_status_countdown = ctx_b.delayed_status_countdown =
	    (int)0x7e7e7e7e;

	memset(tx_a, (int)(unsigned char)POISON_SH, sizeof(tx_a));
	memcpy(tx_b, tx_a, sizeof(tx_a));
	memset(dst_a, POISON_CH, sizeof(dst_a));
	memcpy(dst_b, dst_a, sizeof(dst_a));
	fill(rx_dummy, sizeof(rx_dummy));

	rxc_a = rxc_b = (int)0x3c3c3c3c;
	txc_a = txc_b = (int)0x7e7e7e7e;
	w7_a = w7_b = (int)0x12344321;
	w8_a = w8_b = (int)0x87654321;
}

static void
compare(long tag)
{
	diff_eq_int("ctx (%ld)", memcmp(&ctx_a, &ctx_b, sizeof(ctx_a)), 0,
		    tag);
	diff_eq_int("tx block + guard (%ld)", memcmp(tx_a, tx_b, sizeof(tx_a)),
		    0, tag);
	diff_eq_int("dst block + guard (%ld)",
		    memcmp(dst_a, dst_b, sizeof(dst_a)), 0, tag);
	diff_eq_int("tx_count (%ld)", (long)txc_b, (long)txc_a, tag);
	diff_eq_int("rx_count untouched (%ld)", (long)rxc_b, (long)rxc_a, tag);
	diff_eq_int("word7 (%ld)", (long)w7_b, (long)w7_a, tag);
	diff_eq_int("word8 untouched (%ld)", (long)w8_b, (long)w8_a, tag);
}

static int
run(void)
{
	static const struct {
		int nrec, next, old_f12c8, prev_state;
	} cases[] = {
		/* not expired (old > 0), first tick, unsent record left */
		{ 3, 0, 1, 3 },
		{ 3, 1, 5, 0 },
		/* not expired, continuation (skips the CONNECT write) */
		{ 3, 0, 2, CLASS1_HDLC_EMULATE_RECEIVE_STATE },
		/* not expired, already all caught up -> reset fires anyway */
		{ 3, 3, 1, 3 },
		{ 0, 0, 4, 3 },
		/* expired (old <= 0), still one to send -> emit */
		{ 1, 0, 0, 3 },
		{ 3, 0, -1, CLASS1_HDLC_EMULATE_RECEIVE_STATE },
		{ 3, 2, 0, 3 },
		/* expired, emitting the LAST record -> emit, then reset */
		{ 1, 0, -5, 3 },
		{ 3, 2, 0, CLASS1_HDLC_EMULATE_RECEIVE_STATE },
		/* expired, nothing left (next == count) -> idle + reset */
		{ 3, 3, 0, 3 },
		{ 0, 0, -3, CLASS1_HDLC_EMULATE_RECEIVE_STATE },
		/* expired, next PAST count (overshoot) -> idle, no reset */
		{ 2, 5, 0, 3 },
		/* the maximum record count the object's own table allows */
		{ CLASS1_EMU_MAX_FRAMES, 0, -1, 3 },
		{ CLASS1_EMU_MAX_FRAMES, CLASS1_EMU_MAX_FRAMES - 1, 0, 3 },
	};
	unsigned i;
	int saw_connect = 0, saw_idle_no_carrier = 0, saw_emit = 0,
	    saw_reset = 0, saw_no_reset = 0;

	diff_begin("_hdlc_emulate_receive_state");

	for (i = 0; i < sizeof(cases) / sizeof(cases[0]); i++) {
		int ra, rb;
		long tag = (long)i;

		plant(cases[i].nrec, cases[i].next, cases[i].old_f12c8,
		      cases[i].prev_state);

		ra = ref__hdlc_emulate_receive_state(&ctx_a, rx_dummy, tx_a,
		    (int)(long)dst_a, 0x22222222, &rxc_a, &txc_a,
		    (int)(long)&w7_a, &w8_a);
		rb = _hdlc_emulate_receive_state(&ctx_b, rx_dummy, tx_b,
		    (int)(long)dst_b, 0x22222222, &rxc_b, &txc_b,
		    (int)(long)&w7_b, &w8_b);

		diff_eq_int("return (%ld)", rb, ra, tag);
		compare(tag);

		if (ctx_a.status == FAX_CLASS1_CONNECT)
			saw_connect++;
		if (ctx_a.status == FAX_CLASS1_NO_CARRIER_NO_MESSAGE &&
		    ctx_a.state == CLASS1_IDLE_STATE)
			saw_idle_no_carrier++;
		if (w7_a != (int)0x12344321)
			saw_emit++;
		if (ctx_a.superframe_len == 0 && ctx_a.superframe_countdown == 2 && ctx_a.superframe_read_idx == 0)
			saw_reset++;
		else
			saw_no_reset++;

		diff_eq_int("tx_count is always a full block (%ld)", txc_a,
			    CLASS1_BLOCK_SAMPLES, tag);
	}

	diff_eq_int("the CONNECT status write ran", saw_connect > 0, 1,
		    (long)saw_connect);
	diff_eq_int("the NO_CARRIER_NO_MESSAGE/IDLE transition ran",
		    saw_idle_no_carrier > 0, 1, (long)saw_idle_no_carrier);
	diff_eq_int("a record was emitted (word7 written)", saw_emit > 0, 1,
		    (long)saw_emit);
	diff_eq_int("the buffer reset ran", saw_reset > 0, 1, (long)saw_reset);
	diff_eq_int("and did not, elsewhere", saw_no_reset > 0, 1,
		    (long)saw_no_reset);

	return diff_end();
}

int
main(void)
{
	return run();
}

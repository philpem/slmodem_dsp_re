/*
 * t_class1hdlcemu.c -- differential test of `_hdlc_emulate_receive_state`.
 *
 * `.text` 0x09e1b0, 452 bytes -- HDLC_EMULATE_RECEIVE_STATE, state 7.  It
 * replays length-prefixed records out of `ctx->superframe` (class1.h) one per
 * call, gated by the between-record countdown `superframe_countdown`.  The fixture builds a
 * structurally valid synthetic run of records in `superframe` -- never more
 * than `CLASS1_EMU_MAX_FRAMES`
 * of them, matching the object's own unguarded stack table (class1.h's own
 * comment on that constant) -- and sweeps `superframe_read_idx` (which record is next),
 * `superframe_countdown` (whether the countdown has expired) and `prev_state` (first tick
 * into this state or a continuation) across every combination the function's
 * control flow distinguishes. These planted states do not establish a
 * reachable modem history. The constructed histories below have their own
 * explicitly stated component boundaries.
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
#include "dsplib/debug.h"
#include "dsplib/faxvmi.h"
#include "dsplib/v21cfg.h"

extern struct faxvmi *ref_FAXVMI_create(struct faxvmi *, const struct faxvmi_cfg *);
extern void ref_FAXVMI_delete(struct faxvmi *);
extern int ref_faxvmi_hdlc_frame(struct faxvmi *, unsigned short *, short);
extern int ref_faxvmi_hdlc_unframe(struct faxvmi *, unsigned short *, short);

extern unsigned int ref_dsplibs_debug_level;

extern int ref__hdlc_emulate_receive_state(void *ctx, const short *rx,
					   short *tx, unsigned char *word3, const unsigned char *word4,
					   int *rx_count, int *tx_count,
					   int *word7, int *word8);

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
		    dst_a, (const unsigned char *)(long)0x22222222, &rxc_a, &txc_a,
		    &w7_a, &w8_a);
		rb = _hdlc_emulate_receive_state(&ctx_b, rx_dummy, tx_b,
		    dst_b, (const unsigned char *)(long)0x22222222, &rxc_b, &txc_b,
		    &w7_b, &w8_b);

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

/* Synthetic component queue probe: valid length-prefixed records and clocks,
 * with state/countdown supplied explicitly. This observes the handler's
 * transcript boundary; it does not establish a modem receive lifecycle. */
static int
run_diagnostic(void)
{
	unsigned int level;
	int emit;

	diff_begin("HDLC emulate receive diagnostic boundary");
	for (level = 0; level < 4; level++)
	    for (emit = 0; emit < 2; emit++) {
		long tag = 2 * level + emit;
		int ra, rb;

		plant(1, 0, emit ? 0 : 1, CLASS1_HDLC_EMULATE_RECEIVE_STATE);
		ctx_a.clock_sec = ctx_b.clock_sec = 3;
		ctx_a.clock_frac = ctx_b.clock_frac = 7;
		dsplib_debug_capture_reset();
		dsplibs_debug_level = ref_dsplibs_debug_level = level;
		dsplib_debug_capture_on = 1;
		ra = ref__hdlc_emulate_receive_state(&ctx_a, rx_dummy, tx_a,
		    dst_a, 0, &rxc_a, &txc_a, &w7_a, &w8_a);
		rb = _hdlc_emulate_receive_state(&ctx_b, rx_dummy, tx_b,
		    dst_b, 0, &rxc_b, &txc_b, &w7_b, &w8_b);
		dsplib_debug_capture_on = 0;
		dsplibs_debug_level = ref_dsplibs_debug_level = 0;
		diff_eq_int("diagnostic transcript (%ld)",
		    strcmp(dsplib_debug_capture_text(0), dsplib_debug_capture_text(1)), 0, tag);
		diff_eq_int("reference emission diagnostic (%ld)",
		    strstr(dsplib_debug_capture_text(1),
		        " 3.07[sec] Receive buffer OK in _hdlc_emulate_receive_state\n") != NULL,
		    level > 1 && emit, tag);
		diff_eq_int("diagnostic return (%ld)", rb, ra, tag);
		compare(tag);
	    }
	return diff_end();
}

extern struct fax_class1 *ref_fax_class1_create(struct fax_class1 *, const struct fax_class1_cfg *);
extern int ref_fax_class1_command(struct fax_class1 *, int, int, int);
extern int ref_fax_class1_progress(struct fax_class1 *, short *, short *, unsigned char *,
	const unsigned char *,
                                 int *, int *, int *, int *);
extern int ref_fax_class1_delete(struct fax_class1 *);

/* Constructor/command/progress history from a buffered-frame boundary. Only
 * the queue payload and its length are supplied; state, read index, countdown
 * and clocks come from production methods. The queue's demodulator/HDLC
 * producer is still outside this fixture: this is not modem-path evidence. */
/* Constructed HDLC components joined at their digital bit-buffer boundary.
 * The caller supplies payloads, never framer state or FCS.  This deliberately
 * bypasses V.21 PCM acquisition and the class-1 state-6 queue writer; copying
 * the decoded words into superframe below is a declared fixture boundary.
 * Original state-6 publication: 9dc24 reads the unsigned word length,
 * 9dc3f publishes it at owner+1000+2*index, 9dc50/9dc58 copy words,
 * and 9dc6c publishes the new element count. The records include FCS;
 * the unpacker publishes its full assembled frame length without stripping it.
 */
static int
produce_records(struct fax_class1 *a, struct fax_class1 *b)
{
    struct faxvmi_cfg tc = FAXVMI_CFG, rc = FAXVMI_CFG;
    struct faxvmi *tx[2], *rx[2];
    unsigned short input[2][13] = {
        {2, 0xff, 0x03, 3, 0xff, 0x03, 0x7e, 5, 0xff, 0x03, 0xff, 0xf8, 0x00},
        {2, 0xff, 0x03, 3, 0xff, 0x03, 0x7e, 5, 0xff, 0x03, 0xff, 0xf8, 0x00}
    };
    unsigned short queue[2][64];
    int words[2] = {0, 0}, frames[2] = {0, 0};
    int tick, side;
    tc.mode = rc.mode = FAXVMI_MODE_HDLC;
    tc.reverse = rc.reverse = 0;
    tc.fifo_size = rc.fifo_size = 128;
    tc.max_frame = rc.max_frame = 96;
    tc.frame_size = rc.frame_size = 96;
    tc.slot = 5; rc.slot = 6;
    tc.modem_cfg = &V21TX_CFG; rc.modem_cfg = &V21RX_CFG;
    tx[0] = ref_FAXVMI_create(NULL, &tc); tx[1] = FAXVMI_create(NULL, &tc);
    rx[0] = ref_FAXVMI_create(NULL, &rc); rx[1] = FAXVMI_create(NULL, &rc);
    diff_eq_int("four framing constructors succeeded", tx[0] && tx[1] && rx[0] && rx[1], 1, 0);
    if (!tx[0] || !tx[1] || !rx[0] || !rx[1]) return 0;
    memset(queue, 0, sizeof(queue));
    for (tick = 0; tick < 100; tick++) {
        unsigned short decoded[2][64];
        int n[2], got[2];
        memset(decoded, 0xa5, sizeof(decoded));
        n[0] = ref_faxvmi_hdlc_frame(tx[0], input[0], tick == 0 ? 3 : 0);
        n[1] = faxvmi_hdlc_frame(tx[1], input[1], tick == 0 ? 3 : 0);
        diff_eq_int("packed bit count %ld", n[1], n[0], tick);
        diff_eq_int("six bits per pack %ld", n[0], 6, tick);
        diff_eq_obj("packed bits", unsigned short[6], tx[0]->link->buf, tx[1]->link->buf, tick);
        for (side = 0; side < 2; side++)
            memcpy(rx[side]->link->buf, tx[side]->link->buf, 6 * sizeof(unsigned short));
        got[0] = ref_faxvmi_hdlc_unframe(rx[0], decoded[0], n[0]);
        got[1] = faxvmi_hdlc_unframe(rx[1], decoded[1], n[1]);
        diff_eq_int("decoded frame count %ld", got[1], got[0], tick);
        diff_eq_obj("decoded records", unsigned short[64], decoded[0], decoded[1], tick);
        for (side = 0; side < 2; side++) {
            int j, off = 0;
            if (got[side] < 0 || got[side] > 1) return 0;
            for (j = 0; j < got[side]; j++) {
                int k, len = decoded[side][off];
                if (len > 16 || words[side] + len + 1 > 64) return 0;
                for (k = 0; k <= len; k++) queue[side][words[side]++] = decoded[side][off++];
                frames[side]++;
            }
        }
    }
    diff_eq_int("reference produced three records", frames[0], 3, 0);
    diff_eq_int("source produced three records", frames[1], 3, 0);
    diff_eq_int("payloads plus FCS and lengths", words[0], 19, 0);
    for (side = 0; side < 2; side++) {
        static const unsigned short expected[13] =
            {2, 0xff, 0x03, 3, 0xff, 0x03, 0x7e, 5, 0xff, 0x03, 0xff, 0xf8, 0x00};
        int record, src = 0, dst = 0;
        for (record = 0; record < 3; record++) {
            int k, len = expected[src++];
            diff_eq_int("decoded payload plus FCS length %ld", queue[side][dst++], len + 2, side*10+record);
            for (k = 0; k < len; k++)
                diff_eq_int("decoded payload octet %ld", queue[side][dst++], expected[src++], side*100+record*10+k);
            dst += 2;
        }
    }
    diff_eq_obj("produced queue", unsigned short[64], queue[0], queue[1], 0);
    memcpy(a->superframe, queue[0], words[0] * sizeof(unsigned short)); a->superframe_len = words[0];
    memcpy(b->superframe, queue[1], words[1] * sizeof(unsigned short)); b->superframe_len = words[1];
    ref_FAXVMI_delete(tx[0]); FAXVMI_delete(tx[1]);
    ref_FAXVMI_delete(rx[0]); FAXVMI_delete(rx[1]);
    return frames[0] == 3 && frames[1] == 3;
}

static int
run_command_history(int framed)
{
    struct fax_class1_cfg cfg = { CLASS1_ANS_ORG_NORMAL, 60, 0, 0, 0, 1 };
    unsigned level;
    int round, tick;
    unsigned emitted = 0;

    diff_begin(framed ? "HDLC framing to command/progress (digital bit boundary)"
                      : "buffered HDLC command/progress history (queue supplied)");
    for (level = 0; level < 4; level++) {
        struct fax_class1 *a = ref_fax_class1_create(NULL, &cfg);
        struct fax_class1 *b = fax_class1_create(NULL, &cfg);
        int i;
        diff_eq_int("constructors succeeded %ld", a != NULL && b != NULL, 1, level);
        if (a == NULL || b == NULL) return diff_end() | 1;
        for (i = 0; i < 3; i++) {
            a->superframe[3*i] = b->superframe[3*i] = 2;
            a->superframe[3*i+1] = b->superframe[3*i+1] = 0x10;
            a->superframe[3*i+2] = b->superframe[3*i+2] = 0x30+i;
        }
        a->superframe_len = b->superframe_len = 9;
        if (framed && !produce_records(a, b)) return diff_end() | 1;
        for (round = 0; round < 3; round++) {
            int ra = ref_fax_class1_command(a, FAX_CLASS1_RH_COMMAND, 3, 0);
            int rb = fax_class1_command(b, FAX_CLASS1_RH_COMMAND, 3, 0);
            long tag = level*100 + round*10;
            diff_eq_int("command return %ld", rb, ra, tag);
            diff_eq_int("reference selected emulate %ld", a->state, CLASS1_HDLC_EMULATE_RECEIVE_STATE, tag);
            diff_eq_int("source selected emulate %ld", b->state, a->state, tag);
            diff_eq_int("command armed countdown %ld", b->superframe_countdown, 2, tag);
            for (tick = 0; tick < 3; tick++) {
                short arx[CLASS1_BLOCK_SAMPLES], brx[CLASS1_BLOCK_SAMPLES];
                memset(arx, 0, sizeof(arx)); memset(brx, 0, sizeof(brx));
                memset(tx_a, 0x5a, sizeof(tx_a)); memset(tx_b, 0x5a, sizeof(tx_b));
                memset(dst_a, 0xa5, sizeof(dst_a)); memset(dst_b, 0xa5, sizeof(dst_b));
                rxc_a = rxc_b = CLASS1_BLOCK_SAMPLES;
                txc_a = txc_b = CLASS1_BLOCK_SAMPLES;
                w7_a = w7_b = w8_a = w8_b = 0;
                dsplib_debug_capture_reset();
                dsplibs_debug_level = ref_dsplibs_debug_level = level;
                dsplib_debug_capture_on = 1;
                ra = ref_fax_class1_progress(a, arx, tx_a, dst_a, 0,
                    &rxc_a, &txc_a, &w7_a, &w8_a);
                rb = fax_class1_progress(b, brx, tx_b, dst_b, 0,
                    &rxc_b, &txc_b, &w7_b, &w8_b);
                dsplib_debug_capture_on = 0;
                dsplibs_debug_level = ref_dsplibs_debug_level = 0;
                diff_eq_int("progress return %ld", rb, ra, tag+tick);
                diff_eq_int("transcript complete %ld", dsplib_debug_capture_complete(0) && dsplib_debug_capture_complete(1), 1, tag+tick);
                diff_eq_int("history transcript %ld", strcmp(dsplib_debug_capture_text(0), dsplib_debug_capture_text(1)), 0, tag+tick);
                diff_eq_int("reference report at expiry %ld", strstr(dsplib_debug_capture_text(1), "Receive buffer OK in _hdlc_emulate_receive_state") != NULL, level > 1 && tick == 2, tag+tick);
                diff_eq_int("reference emitted at expiry %ld", w7_a > 0, tick == 2, tag+tick);
                emitted += w7_a > 0;
                diff_eq_int("output count %ld", w7_b, w7_a, tag+tick);
                diff_eq_obj("history output", unsigned char[DST_MAX + GUARD], dst_a, dst_b, tag+tick);
                diff_eq_obj("history tx", short[TX_MAX + GUARD], tx_a, tx_b, tag+tick);
#define HISTORY_FIELD(field) diff_eq_int("history " #field " %ld", b->field, a->field, tag+tick)
                HISTORY_FIELD(state); HISTORY_FIELD(prev_state); HISTORY_FIELD(status);
                HISTORY_FIELD(superframe_countdown); HISTORY_FIELD(superframe_read_idx);
                HISTORY_FIELD(superframe_len); HISTORY_FIELD(delayed_status);
                HISTORY_FIELD(delayed_status_countdown); HISTORY_FIELD(clock_sec);
                HISTORY_FIELD(clock_frac);
#undef HISTORY_FIELD
            }
        }
        diff_eq_int("queue exhausted %ld", b->superframe_len, 0, level);
        diff_eq_int("read index reset %ld", b->superframe_read_idx, 0, level);
        ref_fax_class1_delete(a); fax_class1_delete(b);
    }
    diff_eq_int("twelve buffered records emitted", emitted, 12, 0);
    return diff_end();
}

int
main(void)
{
	int rc = run();

	rc |= run_diagnostic();
	rc |= run_command_history(0);
	rc |= run_command_history(1);
	return rc;
}

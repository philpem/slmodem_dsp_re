/*
 * t_voiceproc.c -- differential test of `VOICE_process` (0xbd0, 2016 bytes),
 * src/service/voice.c.  The other three of the group are in t_voiceapi.
 *
 * HOW THE FOURTEEN MESSAGE ARMS ARE REACHED, and why this is a fixture
 * technique rather than a cheat.  `VOICE_process` dispatches on whatever
 * `voice_modem` answers, and `voice_modem` answers `_handle_status(r, code)`
 * where `r` is the return of the per-block handler in `voice_ctx.handler`.
 * That field is an ordinary function pointer this tree has modelled and
 * t_voicesvc already asserts, so the fixture PLANTS ITS OWN handler -- one per
 * side, so the two cannot consume each other's script -- and drives every
 * message 0..13 and several beyond.  Reaching them through the real handlers
 * would mean re-driving the whole call-progress detector, which t_detector
 * already does; what is under test here is the dispatch, not the detector.
 *
 * WHAT A ONE-BLOCK FIXTURE CANNOT SEE (F8790), and what is done about it:
 *
 *   - `VOICE_process` has TWO nested loops and TWO ring buffers, so every
 *     case here runs a whole sweep of sample counts, including several past
 *     the block size so the OUTER loop iterates more than once, and several
 *     that are not a multiple of it.
 *   - the rings only wrap after 2*block samples have gone through, so the
 *     cursors are also planted directly at offsets that force the
 *     "distance to the end of the window" clamp to bite on the very first
 *     memcpy, and the wrap is then asserted to have HAPPENED rather than
 *     assumed.
 *
 * `ret` IS SET BY ONE ARM AND OVERWRITTEN BY ANOTHER (F8812's shape, exactly).
 * A run whose last block answers a status hides a wrong status from an earlier
 * block, because `pending` is assigned unconditionally whenever the code is
 * non-zero.  So every message script here is also driven with the status-
 * bearing message in the MIDDLE of a multi-block run and a silent message
 * last, and the return is asserted for both orders.
 *
 * WHAT IS COMPARED AFTER EVERY CALL: the return, the whole 0x1484-byte object
 * with only the three necessarily-different pointers zeroed, the voice service
 * core underneath it, both caller buffers byte for byte INCLUDING the guard
 * region past what should have been touched, the host TTY transcript in both
 * directions, and the debug transcript.
 *
 * ONLY 9600 Hz IS DRIVEN, AND THAT IS THE OBJECT'S DOING.  48 kHz indexes
 * 1,536 samples past the end of its own ring (D1023) and 8 kHz builds no rate
 * converters at all, which makes the blob's `RcFixed_Resample` fault on the
 * first complete block (D1022) -- found by running it, not by reading it.
 * `t_rate_bounds` asserts the precondition for each, on both sides where
 * there are two sides to compare, rather than leaving the gap unexplained.
 * Finding F8838.
 */

#include <stdio.h>
#include <string.h>

#include "harness.h"
#include "dsplib/beepgen.h"
#include "dsplib/debug.h"
#include "dsplib/detector.h"
#include "dsplib/fifo8.h"
#include "dsplib/modem_params.h"
#include "dsplib/silence.h"
#include "dsplib/vce.h"
#include "dsplib/voice.h"

extern unsigned int ref_dsplibs_debug_level;

extern void *ref_VOICE_create(void *modem, unsigned int rate);
extern void ref_VOICE_delete(void *obj);
extern int ref_VOICE_process(void *obj, void *in, void *out, int count);

extern int dsplib_debug_capture_on;
void dsplib_debug_capture_reset(void);
unsigned dsplib_debug_capture_lines(int side);
const char *dsplib_debug_capture_text(int side);

/* ------------------------------------------------------------------ */
/* The host side.                                                     */

static int host_modem;
static struct voice_info host_info;

static void
fill_info(void)
{
	host_info.comp_method = 101;
	host_info.sample_rate = 8000;
	host_info.rx_gain = 103;
	host_info.tx_gain = 104;
	host_info.dtmf_symbol = 105;
	host_info.tone1_freq = 941;
	host_info.tone2_freq = 1336;
	host_info.tone_duration = 250;
	host_info.inactivity_timer = 109;
	host_info.silence_detect_sensitivity = 110;
	host_info.silence_detect_period = 111;
}

static void
set_params(void)
{
	unsigned k;
	static const int on_off[4] = {
		GetMaxBusyCadenceOnTime, GetMinBusyCadenceOnTime,
		GetMinBusyCadenceOffTime, GetMaxBusyCadenceOffTime
	};

	harness_param_reset();
	harness_param_set(MDMPRM_VOICEINFO, (long)(size_t)&host_info);
	harness_param_set(GetDialToneCallProgressFilterIndex, 1);
	harness_param_set(GetBusyToneCallProgressFilterIndex, 1);
	harness_param_set(GetCongestionToneCallProgressFilterIndex, 1);
	harness_param_set(GetRingbackToneCallProgressFilterIndex, 1);
	harness_param_set(GetDialToneFilterSubindex, 0);
	harness_param_set(GetCallProgressSamplesBufferLength, 666);
	harness_param_set(GetDialToneValidationTime, 50);
	harness_param_set(GetDialToneDetectionThreshold, 40);
	harness_param_set(GetBusyToneLooseDetectionEnabled, 0);
	harness_param_set(GetBusyDetectionCyclesNumber, 3);
	harness_param_set(GetCongestionDetectionCyclesNumber, 3);
	harness_param_set(GetRingbackDetectionCyclesNumber, 3);
	harness_param_set(GetBusyToneDiffTime, 3);
	for (k = 0; k < 4; k++)
		harness_param_set(on_off[k], (int)(60 + k * 13));
}

static void
set_level(unsigned int lvl)
{
	dsplibs_debug_level = lvl;
	ref_dsplibs_debug_level = lvl;
}

/* ------------------------------------------------------------------ */
/* The planted per-block handler: one per side, one cursor each.      */

#define SCRIPT_MAX	16

static int script_msg[SCRIPT_MAX];
static int script_len;
static unsigned short script_hostcount = 4;
static unsigned short script_count = 8;

static int cursor[2];
static long handler_calls[2];
static int prev_msg[2];
static long cov_short_circuit;
static long cov_handler_audio;

static int
run_handler(int side, struct voice_ctx *v, short *rx_lin, float *rx_flt,
	    float *tx_flt, short *tx_lin, unsigned short *hostcount,
	    unsigned short *countp)
{
	unsigned char *out = (unsigned char *)tx_lin;
	int n = (int)*countp;
	int msg, i;

	(void)v;
	(void)rx_lin;

	/*
	 * Carry the line's own samples through, transformed, so the whole
	 * audio path -- resample in, scale to float, handler, scale back,
	 * resample out -- is observable in the caller's output buffer.  The
	 * scale keeps every value well inside `short` after the 16000x on the
	 * way out, so no conversion here is at the edge of the cast.
	 */
	if (n > 160)
		n = 160;
	for (i = 0; i < n; i++)
		rx_flt[i] = tx_flt[i] * 0.5f
			    + (float)(i % 7) * (1.0f / 4096.0f);
	if (n > 0)
		cov_handler_audio++;

	/* A deterministic host block, so the RX send has something to send. */
	for (i = 0; i < 16; i++)
		out[i] = (unsigned char)(0x40 + i + side * 0);

	*hostcount = script_hostcount;
	*countp = script_count;

	msg = script_msg[cursor[side] % script_len];
	cursor[side]++;
	if (msg == prev_msg[side])
		cov_short_circuit++;
	prev_msg[side] = msg;
	handler_calls[side]++;
	return msg;
}

static int
handler_ours(struct voice_ctx *v, short *rx_lin, float *rx_flt, float *tx_flt,
	     short *tx_lin, unsigned short *hostcount, unsigned short *countp)
{
	return run_handler(0, v, rx_lin, rx_flt, tx_flt, tx_lin, hostcount,
			   countp);
}

static int
handler_ref(struct voice_ctx *v, short *rx_lin, float *rx_flt, float *tx_flt,
	    short *tx_lin, unsigned short *hostcount, unsigned short *countp)
{
	return run_handler(1, v, rx_lin, rx_flt, tx_flt, tx_lin, hostcount,
			   countp);
}

static void
script_reset(const int *msgs, int n)
{
	int i;

	for (i = 0; i < n && i < SCRIPT_MAX; i++)
		script_msg[i] = msgs[i];
	script_len = (n < SCRIPT_MAX) ? n : SCRIPT_MAX;
	cursor[0] = cursor[1] = 0;
	/*
	 * `last_message` is 0 in a freshly created object, so the "previous
	 * message" the short-circuit compares against starts at 0 here too.
	 */
	prev_msg[0] = prev_msg[1] = 0;
}

/* ------------------------------------------------------------------ */

#define TEXTBUF		32768

static long cov_printed;

/* ------------------------------------------------------------------ */
/* Comparison.                                                        */

static long objects_compared, calls_made;

static void
normalise_vce(struct vce *v)
{
	v->voice = 0;
	v->rc_in = 0;
	v->rc_out = 0;
}

static void
normalise_ctx(struct voice_ctx *c)
{
	c->beepgen = 0;
	c->detector = 0;
	c->fifo = 0;
	c->silence = 0;
	c->dp = 0;
	c->handler = 0;
	c->cfg.get_sreg = 0;
	c->cfg.hook_on = 0;
	c->cfg.hook_off = 0;
}

static void
normalise_detector(struct detector *d)
{
	int i;

	d->dtmf = 0;
	d->cadence_0008 = 0;
	d->cadence_busy = 0;
	d->cadence_dial = 0;
	for (i = 0; i < DETECTOR_TONES; i++)
		d->tone[i] = 0;
}

static void
compare_vce(struct vce *ours, struct vce *ref, long tag)
{
	struct vce a, b;
	struct voice_ctx ca, cb;
	struct detector da, db;

	a = *ours;
	b = *ref;
	normalise_vce(&a);
	normalise_vce(&b);
	diff_eq_obj("vce", struct vce, &a, &b, tag);
	objects_compared++;

	ca = *ours->voice;
	cb = *ref->voice;
	normalise_ctx(&ca);
	normalise_ctx(&cb);
	diff_eq_obj("voice_ctx", struct voice_ctx, &ca, &cb, tag);

	/*
	 * The detector too, because `voice_modem` runs it on every block and
	 * a wrong sample buffer would show here and nowhere else.  Its own
	 * sub-object pointers are each side's own heap, so they go the same
	 * way as the context's.
	 */
	da = *ours->voice->detector;
	db = *ref->voice->detector;
	normalise_detector(&da);
	normalise_detector(&db);
	diff_eq_obj("detector", struct detector, &da, &db, tag);
}

/* ------------------------------------------------------------------ */
/* The caller's buffers, guarded.                                     */

#define BUFSHORTS	4096
#define GUARD		0x5a

static short in_ours[BUFSHORTS], in_ref[BUFSHORTS];
static short out_ours[BUFSHORTS], out_ref[BUFSHORTS];

static void
fill_in(short *p, int seed)
{
	int i;

	for (i = 0; i < BUFSHORTS; i++)
		p[i] = (short)(((i * 37 + seed * 11) % 4001) - 2000);
}

static void
fill_out(short *p)
{
	memset(p, GUARD, sizeof(short) * BUFSHORTS);
}

static long cov_guard_checked;

/*
 * The byte the object should never reach, and the one it should.  The inner
 * loop consumes `2 * chunk` bytes per outer step but the outer step advances
 * the caller's pointers by `chunk` (D1020), so the highest byte written is
 * `(iterations - 1) * block + 2 * chunk_last` -- not `2 * count`.
 */
static int
touched_bytes(int count, int block)
{
	int base = 0;
	int high = 0;

	while (count > 0) {
		int chunk = count < block ? count : block;

		if (base + 2 * chunk > high)
			high = base + 2 * chunk;
		base += chunk;
		count -= chunk;
	}
	return high;
}

static void
check_buffers(int count, int block, long tag)
{
	int high = touched_bytes(count, block);
	unsigned char *a = (unsigned char *)out_ours;
	unsigned char *b = (unsigned char *)out_ref;
	int i, differ = -1, guard_ok = 1;

	for (i = 0; i < (int)sizeof(out_ours); i++)
		if (a[i] != b[i]) {
			differ = i;
			break;
		}
	diff_eq_int("output buffers agree byte for byte", differ, -1, tag);

	for (i = high; i < (int)sizeof(out_ours); i++)
		if (a[i] != GUARD) {
			guard_ok = 0;
			break;
		}
	diff_eq_int("nothing written past the reachable high-water mark",
		    guard_ok, 1, tag);
	cov_guard_checked++;

	differ = -1;
	for (i = 0; i < (int)sizeof(in_ours); i++)
		if (((unsigned char *)in_ours)[i]
		    != ((unsigned char *)in_ref)[i]) {
			differ = i;
			break;
		}
	diff_eq_int("neither side wrote to the input buffer", differ, -1, tag);
}

/* ------------------------------------------------------------------ */

static const char *const msg_line[14] = {
	"voice: STRM_VCE: VOICE_NO_MESSAGE",
	"voice: STRM_VCE:VOICE_OK",
	"voice: STRM_VCE: VOICE_START_ONLINE\n",
	"voice: STRM_VCE:VOICE_START_TX",
	"voice: STRM_VCE:VOICE_START_RX",
	"voice: STRM_VCE:VOICE_START_DUPLEX",
	"voice: STRM_VCE:VOICE_PURGE",
	"voice: STRM_VCE:VOICE_ERROR",
	"voice: STRM_VCE: VOICE_START_ONLINE_AFTER_ABORT(1)",
	"voice: STRM_VCE:VOICE_CANCEL",
	"voice: STRM_VCE: BUSY",
	"voice: STRM_VCE: DIALTONE",
	"voice: STRM_VCE: FAX Tone",
	"voice: STRM_VCE: Underrun"
};
static long cov_msg[14];
static long cov_unknown;
static long cov_outer_multi, cov_outer_single;
static long cov_wrap;
static long cov_rate[1];
static long cov_report_bytes;

/*
 * ONLY 9600 IS DRIVEN THROUGH `VOICE_process`, AND THAT IS THE OBJECT'S OWN
 * DOING RATHER THAN A GAP IN THIS FIXTURE.  The other two rates each break
 * the function in a different way, both proved in `t_rate_bounds` below:
 *
 *   8000  -- `VOICE_create` builds NEITHER converter, and the object's
 *            `RcFixed_Resample` dereferences its handle on its fourth
 *            instruction (`mov (%edx),%ecx` at 0xb12bf, with no NULL test
 *            anywhere before it).  So the blob FAULTS on the first complete
 *            block.  Our src/core/FixedRC.c tolerates a NULL handle -- a
 *            pre-existing, documented tolerance in that file -- so the two
 *            sides genuinely cannot be compared here.  Deviation D1022.
 *   48000 -- `block` is 960, the cursors wrap modulo 1920, and the ring's
 *            array is 384 samples.  Deviation D1023.
 *
 * 9600 is the rate this whole module is written for -- fixedrc.h's banner
 * says the host runs at 9600 -- and it is the one rate at which `2 * block`
 * is exactly the ring's array.  Finding F8838.
 */
static const unsigned int rates[1] = { VCE_RATE_9600 };
#define NRATES	1

/*
 * One trial: two fresh objects, a scripted message sequence, one
 * `VOICE_process` call per side, everything compared.
 */
static void
trial(unsigned int rate, int count, unsigned int lvl, const int *msgs,
      int nmsgs, unsigned int plant_wr, unsigned int plant_rd,
      const unsigned char *ttyin, int ttyin_len, int state, long tag)
{
	struct vce *a, *b;
	char text_a[TEXTBUF], text_b[TEXTBUF];
	int ra, rb, i;
	unsigned int wr_before;

	harness_alloc_reset();
	set_params();
	fill_info();
	set_level(0);

	b = (struct vce *)ref_VOICE_create(&host_modem, rate);
	a = (struct vce *)VOICE_create(&host_modem, rate);
	if (a == 0 || b == 0) {
		diff_eq_int("create for the process sweep", 0, 1, tag);
		return;
	}

	a->voice->handler = handler_ours;
	b->voice->handler = handler_ref;
	a->state = state;
	b->state = state;
	a->host_count = script_hostcount;
	b->host_count = script_hostcount;

	/*
	 * Plant both cursors, and plant them on BOTH sides identically.  A
	 * cursor is a SUBSCRIPT, and D955/F8587 is precisely that an
	 * unplanted subscript reads the same wild index on both sides and so
	 * agrees while proving nothing.
	 */
	a->in_ring.wr = plant_wr % (2 * a->block);
	b->in_ring.wr = a->in_ring.wr;
	a->out_ring.rd = plant_rd % (2 * a->block);
	b->out_ring.rd = a->out_ring.rd;
	wr_before = a->in_ring.wr;

	script_reset(msgs, nmsgs);

	fill_in(in_ours, count);
	fill_in(in_ref, count);
	fill_out(out_ours);
	fill_out(out_ref);

	harness_tty_reset();
	harness_ttyin_reset(ttyin, ttyin_len);
	set_level(lvl);
	dsplib_debug_capture_reset();
	dsplib_debug_capture_on = 1;

	rb = ref_VOICE_process(b, in_ref, out_ref, count);
	ra = VOICE_process(a, in_ours, out_ours, count);

	dsplib_debug_capture_on = 0;
	set_level(0);
	calls_made++;

	diff_eq_int("return value", ra, rb, tag);
	compare_vce(a, b, tag);
	check_buffers(count, (int)a->block, tag);

	diff_eq_int("tty output length", (long)harness_tty_ours.len,
		    (long)harness_tty_ref.len, tag);
	diff_eq_int("tty output call count", (long)harness_tty_ours.calls,
		    (long)harness_tty_ref.calls, tag);
	diff_eq_int("tty output bytes",
		    memcmp(harness_tty_ours.data, harness_tty_ref.data,
			   sizeof(harness_tty_ours.data)) == 0, 1, tag);
	diff_eq_int("tty input call count", (long)harness_ttyin_ours.calls,
		    (long)harness_ttyin_ref.calls, tag);
	diff_eq_int("tty input bytes taken", (long)harness_ttyin_ours.bytes,
		    (long)harness_ttyin_ref.bytes, tag);
	diff_eq_int("handler call count", handler_calls[0], handler_calls[1],
		    tag);

	/*
	 * THE TRANSCRIPT IS COMPARED RAW, with no filtering at all, and that
	 * is a claim rather than an omission.  Every line this window can
	 * contain is `VOICE_process`'s own; the three uncomparable lines
	 * t_voiceapi has to drop -- the "StrmVCE old:" stack read, the
	 * pointer `voice_delete` prints and the address pair `beepgen_create`
	 * prints -- all come from construction and teardown, which happen
	 * outside the capture window here.  If one ever moves inside it this
	 * comparison fails loudly, which is the right outcome.
	 */
	strncpy(text_a, dsplib_debug_capture_text(0), TEXTBUF - 1);
	text_a[TEXTBUF - 1] = '\0';
	strncpy(text_b, dsplib_debug_capture_text(1), TEXTBUF - 1);
	text_b[TEXTBUF - 1] = '\0';
	diff_eq_int("transcript", strcmp(text_a, text_b) == 0, 1, tag);
	if (dsplib_debug_capture_lines(1) > 0)
		cov_printed++;

	/* Coverage, from the REFERENCE side's own transcript (F134). */
	if (lvl > 1) {
		for (i = 0; i < 14; i++)
			if (strstr(text_b, msg_line[i]) != 0)
				cov_msg[i]++;
		if (strstr(text_b, "voice: STRMVCE Monitor: Unknown message")
		    != 0)
			cov_unknown++;
	}
	if (count > (int)a->block)
		cov_outer_multi++;
	else
		cov_outer_single++;
	if (a->in_ring.wr < wr_before)
		cov_wrap++;
	cov_rate[0]++;
	if (harness_tty_ref.len > 0)
		cov_report_bytes++;

	ref_VOICE_delete(b);
	VOICE_delete(a);
	diff_eq_int("create/delete balance", (long)harness_alloc.live, 0, tag);
}

/* ------------------------------------------------------------------ */

static int
t_messages(void)
{
	unsigned int lvl, r;
	int m;

	diff_begin("every message arm, alone and in the middle of a run");

	for (lvl = 0; lvl <= 2; lvl += 2) {
		for (r = 0; r < NRATES; r++) {
			for (m = 0; m <= 15; m++) {
				int alone[1];
				int middle[3];
				long tag = (long)(lvl * 1000 + r * 100 + m);

				alone[0] = m;
				/*
				 * The status-bearing message in the MIDDLE,
				 * with a silent one last: if `ret` were taken
				 * from the final block rather than latched,
				 * this is the case that says so (F8812).
				 */
				middle[0] = VOICE_NO_MESSAGE;
				middle[1] = m;
				middle[2] = VOICE_PURGE;

				/*
				 * Three blocks of the message on its own --
				 * which also drives the short-circuit, since
				 * blocks two and three repeat it -- and then
				 * four blocks with it in the MIDDLE.
				 */
				trial(rates[r], 3 * (int)(rates[r] * 160 / 8000),
				      lvl, alone, 1, 0, 0, 0, 0,
				      VOICE_STATE_COMMAND, tag);
				trial(rates[r], 4 * (int)(rates[r] * 160 / 8000),
				      lvl, middle, 3, 0, 0, 0, 0,
				      VOICE_STATE_COMMAND, tag + 10000);
			}
		}
	}
	return diff_end();
}

static int
t_counts(void)
{
	static const int counts[] = {
		0, -1, -1000, 1, 2, 3, 159, 160, 161, 191, 192, 193, 200,
		320, 384, 385, 500, 900, 1000
	};
	static const int msgs[2] = { VOICE_NO_MESSAGE, VOICE_START_DUPLEX };
	unsigned int r;
	unsigned c;

	diff_begin("every interesting sample count, either side of the block");

	for (r = 0; r < NRATES; r++)
		for (c = 0; c < sizeof(counts) / sizeof(counts[0]); c++)
			trial(rates[r], counts[c], 0, msgs, 2, 0, 0, 0, 0,
			      VOICE_STATE_COMMAND, (long)(r * 100 + c));
	return diff_end();
}

static int
t_cursors(void)
{
	static const int msgs[1] = { VOICE_NO_MESSAGE };
	unsigned int r;
	unsigned int k;

	diff_begin("the ring cursors, planted right up to the wrap");

	for (r = 0; r < NRATES; r++) {
		unsigned int two_block = 2 * (rates[r] * 160 / 8000);

		for (k = 0; k < 8; k++) {
			unsigned int wr = (two_block - k) % two_block;
			unsigned int rd = (two_block / 2 + k * 3) % two_block;

			trial(rates[r], 700, 0, msgs, 1, wr, rd, 0, 0,
			      VOICE_STATE_COMMAND, (long)(r * 100 + k));
		}
	}
	return diff_end();
}

/*
 * The two host-facing states.  TX pulls `host_count` bytes from the host each
 * block; RX pulls a fixed 160 and, if it got more than one byte and the first
 * two are <DLE>'!', aborts the recording -- and sends the handler's block back
 * to the host.
 */
static long cov_state[4];
static long cov_abort;

static int
t_states(void)
{
	static const int msgs[2] = { VOICE_NO_MESSAGE, VOICE_START_RX };
	static const unsigned char abort_seq[8] = {
		0x10, '!', 0x11, 0x12, 0x13, 0x14, 0x15, 0x16
	};
	static const unsigned char plain_seq[8] = {
		'a', 'b', 'c', 'd', 'e', 'f', 'g', 'h'
	};
	static const unsigned char one_byte[1] = { 0x10 };
	unsigned int lvl;
	int st;

	diff_begin("the four service states, with and without host input");

	for (lvl = 0; lvl <= 2; lvl += 2) {
		for (st = 0; st <= 3; st++) {
			long tag = (long)(lvl * 100 + st);

			trial(VCE_RATE_9600, 400, lvl, msgs, 2, 0, 0,
			      abort_seq, (int)sizeof(abort_seq), st, tag);
			cov_state[st]++;
			if (st == VOICE_STATE_RX)
				cov_abort++;

			trial(VCE_RATE_9600, 400, lvl, msgs, 2, 0, 0,
			      plain_seq, (int)sizeof(plain_seq), st,
			      tag + 1000);
			trial(VCE_RATE_9600, 400, lvl, msgs, 2, 0, 0,
			      one_byte, 1, st, tag + 2000);
			trial(VCE_RATE_9600, 400, lvl, msgs, 2, 0, 0, 0, 0,
			      st, tag + 3000);
		}
	}
	return diff_end();
}

/*
 * D1020, asserted directly rather than only reproduced.  With `count` two
 * blocks wide the object moves `2 * count` BYTES through its rings but
 * advances the caller's pointers by `count` bytes in total, so the last byte
 * it writes is at `3 * block` and not at `4 * block`.  A correct
 * implementation would leave no guard bytes below `4 * block`.
 */
static long cov_underadvance;

static int
t_underadvance(void)
{
	static const int msgs[1] = { VOICE_NO_MESSAGE };
	unsigned int r;

	diff_begin("the outer loop advances the caller's buffers by half");

	for (r = 0; r < NRATES; r++) {
		int block = (int)(rates[r] * 160 / 8000);
		int count = 2 * block;
		unsigned char *p;
		long tag = (long)r;

		trial(rates[r], count, 0, msgs, 1, 0, 0, 0, 0,
		      VOICE_STATE_COMMAND, tag + 500);

		p = (unsigned char *)out_ours;
		diff_eq_int("the byte at 3*block-1 was written",
			    p[3 * block - 1] != GUARD, 1, tag);
		diff_eq_int("the byte at 3*block was NOT written",
			    p[3 * block], GUARD, tag);
		diff_eq_int("nor was the byte a correct pass would have "
			    "written at 4*block-1",
			    p[4 * block - 1], GUARD, tag);
		diff_eq_int("touched_bytes agrees with the object",
			    touched_bytes(count, block), 3 * block, tag);
		cov_underadvance++;
	}
	return diff_end();
}

/*
 * D1023, asserted as arithmetic rather than by running it.  The ring's window
 * is `2 * block` samples and its array is 384, so the type is only
 * self-consistent up to 9600 Hz.
 */
static int
t_rate_bounds(void)
{
	struct vce *a, *b;

	diff_begin("what each rate does to the ring and to the converters");

	/*
	 * D1022, asserted at its CAUSE.  An 8 kHz object has no converters at
	 * all, and the object's `RcFixed_Resample` dereferences its handle on
	 * its fourth instruction with no NULL test anywhere before it -- so
	 * the blob faults on the first complete block.  What is asserted here
	 * is the precondition, on both sides, because the consequence cannot
	 * be run.
	 */
	harness_alloc_reset();
	set_params();
	fill_info();
	set_level(0);
	b = (struct vce *)ref_VOICE_create(&host_modem, VCE_RATE_8000);
	a = (struct vce *)VOICE_create(&host_modem, VCE_RATE_8000);
	diff_eq_int("8000 builds an object", a != 0 ? b != 0 : 0, 1, 8000);
	if (a != 0 ? b != 0 : 0) {
		diff_eq_int("our 8000 rc_in is NULL", a->rc_in == 0, 1, 8000);
		diff_eq_int("our 8000 rc_out is NULL", a->rc_out == 0, 1,
			    8000);
		diff_eq_int("ref 8000 rc_in is NULL", b->rc_in == 0, 1, 8000);
		diff_eq_int("ref 8000 rc_out is NULL", b->rc_out == 0, 1,
			    8000);
		ref_VOICE_delete(b);
		VOICE_delete(a);
	}

	/* And 9600 does build both, which is why it is the rate driven. */
	harness_alloc_reset();
	set_params();
	b = (struct vce *)ref_VOICE_create(&host_modem, VCE_RATE_9600);
	a = (struct vce *)VOICE_create(&host_modem, VCE_RATE_9600);
	diff_eq_int("9600 builds an object", a != 0 ? b != 0 : 0, 1, 9600);
	if (a != 0 ? b != 0 : 0) {
		diff_eq_int("our 9600 rc_in is present", a->rc_in != 0, 1,
			    9600);
		diff_eq_int("our 9600 rc_out is present", a->rc_out != 0, 1,
			    9600);
		diff_eq_int("ref 9600 rc_in is present", b->rc_in != 0, 1,
			    9600);
		diff_eq_int("ref 9600 rc_out is present", b->rc_out != 0, 1,
			    9600);
		ref_VOICE_delete(b);
		VOICE_delete(a);
	}
	set_level(0);

	diff_eq_int("8000 fits", 2 * (8000 * 160 / 8000) <= VCE_RING_SAMPLES,
		    1, 8000);
	diff_eq_int("9600 fits exactly",
		    2 * (9600 * 160 / 8000), VCE_RING_SAMPLES, 9600);
	diff_eq_int("48000 does NOT fit",
		    2 * (48000 * 160 / 8000) > VCE_RING_SAMPLES, 1, 48000);
	diff_eq_int("and by how much",
		    2 * (48000 * 160 / 8000) - VCE_RING_SAMPLES, 1536, 48000);
	diff_eq_int("the ring is 0x310 bytes", (long)sizeof(struct vce_ring),
		    0x310, 0);
	return diff_end();
}

/* ------------------------------------------------------------------ */

static int
t_coverage(void)
{
	int i;

	diff_begin("the sweeps above reached every arm they claim to");

	for (i = 0; i < 14; i++)
		diff_eq_int("message arm %ld fired", cov_msg[i] > 0, 1,
			    (long)i);
	diff_eq_int("the unknown-message arm fired", cov_unknown > 0, 1, 0);
	diff_eq_int("the short-circuit was taken", cov_short_circuit > 0, 1,
		    0);
	diff_eq_int("the outer loop ran more than once", cov_outer_multi > 0,
		    1, 0);
	diff_eq_int("and exactly once", cov_outer_single > 0, 1, 1);
	diff_eq_int("a ring cursor wrapped", cov_wrap > 0, 1, 0);
	diff_eq_int("9600 was driven", cov_rate[0] > 0, 1, 9600);
	diff_eq_int("report characters reached the host",
		    cov_report_bytes > 0, 1, 0);
	for (i = 0; i < 4; i++)
		diff_eq_int("state %ld was driven", cov_state[i] > 0, 1,
			    (long)i);
	diff_eq_int("the RX abort was driven", cov_abort > 0, 1, 0);
	diff_eq_int("the under-advance was asserted", cov_underadvance > 0, 1,
		    0);
	diff_eq_int("the handler saw real audio", cov_handler_audio > 0, 1, 0);
	diff_eq_int("the guard region was checked", cov_guard_checked > 0, 1,
		    0);
	diff_eq_int("the reference actually printed something",
		    cov_printed > 0, 1, 0);
	diff_eq_int("objects were compared", objects_compared > 0, 1, 0);
	diff_eq_int("both handlers were called the same number of times",
		    handler_calls[0], handler_calls[1], 0);

	fprintf(stderr,
		"t_voiceproc: %ld calls, %ld objects compared, %ld handler "
		"calls a side, outer multi/single %ld/%ld, wraps %ld, "
		"short-circuits %ld\n",
		calls_made, objects_compared, handler_calls[0],
		cov_outer_multi, cov_outer_single, cov_wrap,
		cov_short_circuit);
	fprintf(stderr,
		"t_voiceproc: message arms %ld/%ld/%ld/%ld/%ld/%ld/%ld/%ld/"
		"%ld/%ld/%ld/%ld/%ld/%ld, unknown %ld, states %ld/%ld/%ld/%ld,"
		" aborts %ld, reports %ld, printed %ld\n",
		cov_msg[0], cov_msg[1], cov_msg[2], cov_msg[3], cov_msg[4],
		cov_msg[5], cov_msg[6], cov_msg[7], cov_msg[8], cov_msg[9],
		cov_msg[10], cov_msg[11], cov_msg[12], cov_msg[13],
		cov_unknown, cov_state[0], cov_state[1], cov_state[2],
		cov_state[3], cov_abort, cov_report_bytes, cov_printed);
	return diff_end();
}

int
main(void)
{
	int failed = 0;

	failed |= t_rate_bounds();
	failed |= t_counts();
	failed |= t_messages();
	failed |= t_cursors();
	failed |= t_states();
	failed |= t_underadvance();
	failed |= t_coverage();
	return failed;
}

/*
 * t_voiceapi.c -- differential test of `VOICE_create` (0x720),
 * `VOICE_delete` (0x910) and `VOICE_command` (0x9a0), src/service/voice.c.
 *
 * `VOICE_process` is the fourth of the group and has its own binary,
 * t_voiceproc, because its fixture is a different shape entirely.
 *
 * WHAT IS OBSERVED, AND WHY EACH IS NEEDED.
 *
 *   - The whole 0x1484-byte object, word for word, with the four pointers
 *     that MUST differ zeroed and their presence compared separately.  That
 *     is what catches a wrong offset in the rate arithmetic or in the ring
 *     priming, neither of which shows in a return value.
 *   - The allocation size and the allocation BALANCE.  0x1484 is the
 *     object's own `movl $0x1484,(%esp)`, and `struct vce`'s portable half
 *     is asserted in the source; the literal lives here, on the 32-bit
 *     build, exactly as `struct rd`'s 8 lives in t_ringdet.
 *   - The voice service core underneath, which is where `VOICE_command`'s
 *     whole effect lands: every arm but one just translates and forwards.
 *   - The debug transcript at levels 0 and 2, since every gated site in
 *     these three functions is `> 1` and one level cannot tell `> 1` from
 *     `> 0` (debug.h, F150).
 *
 * THE THREE CALLBACKS CANNOT BE COMPARED BY VALUE, and that is F8770 rather
 * than a fixture weakness: `vce_get_sreg`, `vce_hook_on` and `vce_hook_off`
 * are `t` in the object and external here, so `VOICE_create` stores three
 * different addresses on the two sides.  Each side is checked to have stored
 * ITS OWN three, which is the same move t_voicesvc makes for `handler`.
 *
 * COVERAGE IS COUNTED FROM THE RUN (F134).  `VOICE_command`'s eight arms are
 * counted by matching the REFERENCE side's own transcript against the one
 * format string each arm prints -- not from the table of opcodes driven --
 * and `main` fails if any arm fired zero times.  The denominators are printed
 * beside every verdict.
 *
 * ONE ARM IS UNREACHABLE AND IS NAMED RATHER THAN LEFT AS A GAP.
 * `VOICE_create`'s "one converter built, the other failed" teardown cannot be
 * entered: `RcFixed_Create` answers non-NULL for all four modes the function
 * asks for (2, 3, 4 and 5 are all in `fixedRc_UpFact`), so the two ladders
 * either both succeed or both are skipped.  The teardown itself IS reached,
 * by the unsupported-rate path, where both converters stay NULL.  Finding
 * F8836.
 */

#include <stdio.h>
#include <string.h>

#include "harness.h"
#include "dsplib/beepgen.h"
#include "dsplib/cadence.h"
#include "dsplib/debug.h"
#include "dsplib/detector.h"
#include "dsplib/fdspkrnl.h"
#include "dsplib/fixedrc.h"
#include "dsplib/fifo8.h"
#include "dsplib/modem_params.h"
#include "dsplib/silence.h"
#include "dsplib/vce.h"
#include "dsplib/voice.h"

extern unsigned int ref_dsplibs_debug_level;

extern void *ref_VOICE_create(void *modem, unsigned int rate);
extern void ref_VOICE_delete(void *obj);
extern int ref_VOICE_command(void *obj, unsigned int cmd);

extern int ref_vce_get_sreg(void *modem, unsigned int num);
extern void ref_vce_hook_on(void *p);
extern void ref_vce_hook_off(void *p);

extern int ref_voice_online(struct voice_ctx *v, short *rx_lin, float *rx_flt,
			    float *tx_flt, short *tx_lin,
			    unsigned short *hostcount, unsigned short *countp);
extern int ref_voice_rx(struct voice_ctx *v, short *rx_lin, float *rx_flt,
			float *tx_flt, short *tx_lin,
			unsigned short *hostcount, unsigned short *countp);
extern int ref_voice_tx(struct voice_ctx *v, short *rx_lin, float *rx_flt,
			float *tx_flt, short *tx_lin,
			unsigned short *hostcount, unsigned short *countp);
extern int ref_voice_duplex(struct voice_ctx *v, short *rx_lin, float *rx_flt,
			    float *tx_flt, short *tx_lin,
			    unsigned short *hostcount, unsigned short *countp);

extern int dsplib_debug_capture_on;
void dsplib_debug_capture_reset(void);
unsigned dsplib_debug_capture_lines(int side);
const char *dsplib_debug_capture_text(int side);

/* ------------------------------------------------------------------ */

static int host_modem;			/* an opaque handle                */
static struct voice_info host_info;

/*
 * `VOICE_command` reads four members of this block and `vce_get_sreg` reads
 * three more, so every member gets a DISTINCT value: a copy that read the
 * wrong offset then gets a wrong answer rather than the same wrong bytes on
 * both sides (D955 / F8587).
 */
static void
fill_info(unsigned int duration)
{
	host_info.comp_method = 101;
	host_info.sample_rate = 8000;
	host_info.rx_gain = 103;
	host_info.tx_gain = 104;
	host_info.dtmf_symbol = 105;
	host_info.tone1_freq = 941;
	host_info.tone2_freq = 1336;
	host_info.tone_duration = duration;
	host_info.inactivity_timer = 109;
	host_info.silence_detect_sensitivity = 110;
	host_info.silence_detect_period = 111;
}

/*
 * The country parameters `cadence_create` turns into a SUBSCRIPT, reached
 * from here through VOICE_create -> voice_create -> detector_create.  Without
 * them the two sides index different table entries and the cadence objects
 * diverge -- t_voicesvc's note has the full story.  The values are
 * t_detector's and t_voicesvc's, so all three drive the same country.
 */
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

/*
 * THREE DEBUG LINES ON THIS PATH CANNOT BE COMPARED, and none of the three is
 * a fixture weakness.  Each is dropped from both transcripts and COUNTED
 * SEPARATELY, so a filter that silently matched everything shows up as a
 * category with a count of zero rather than as a clean pass (F134).
 *
 *   0  "voice: StrmVCE old:" -- `STRM_VCE_GetFDSPEnvironmentalParams` prints
 *      its two OUTPUT words before writing them (D991), so the line reports
 *      whatever was on that side's stack.
 *   1  "voice delete "       -- `voice_delete` prints the context POINTER
 *      with %lX, and the two sides allocate their own.
 *   2  an address pair       -- `beepgen_create` prints the two callbacks it
 *      just stored, with "%X  %X".  Those are `vce_hook_on` and
 *      `vce_hook_off`, which are `t` in the object and external here, so the
 *      two sides necessarily print different addresses.  F8770 again, this
 *      time in a transcript rather than in a field.
 *
 * The third has no fixed prefix, so it is matched by SHAPE -- a line of hex
 * digits and spaces and nothing else -- which is narrow enough that no other
 * line in this transcript can hit it.
 */
#define DROP_OLD	"voice: StrmVCE old:"
#define DROP_DELETE	"voice delete "
#define TEXTBUF		16384

static long filtered_lines[3];

static int
is_address_pair(const char *s, unsigned len)
{
	unsigned i, digits = 0;

	if (len > 0 && s[len - 1] == '\n')
		len--;
	if (len == 0)
		return 0;
	for (i = 0; i < len; i++) {
		char c = s[i];

		if ((c >= '0' && c <= '9') || (c >= 'A' && c <= 'F'))
			digits++;
		else if (c != ' ')
			return 0;
	}
	return digits > 0;
}

static void
filter_text(char *dst, const char *src)
{
	unsigned n = 0;

	while (*src != '\0') {
		const char *nl = strchr(src, '\n');
		unsigned len = (nl != 0) ? (unsigned)(nl - src) + 1
					 : (unsigned)strlen(src);

		if (strncmp(src, DROP_OLD, sizeof(DROP_OLD) - 1) == 0) {
			filtered_lines[0]++;
		} else if (strncmp(src, DROP_DELETE,
				   sizeof(DROP_DELETE) - 1) == 0) {
			filtered_lines[1]++;
		} else if (is_address_pair(src, len)) {
			filtered_lines[2]++;
		} else if (n + len < TEXTBUF) {
			memcpy(dst + n, src, len);
			n += len;
		}
		src += len;
	}
	dst[n] = '\0';
}

/* ------------------------------------------------------------------ */
/* Comparing two `struct vce`.                                        */

static long objects_compared;

/*
 * Zero every field that necessarily holds a different address on the two
 * sides: the voice service core and the two converters are each side's own
 * heap objects.  `modem` and `info` are NOT zeroed -- both sides are handed
 * the same handle and `modem_get_param` answers both with the same block, so
 * those two are real comparisons.
 */
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
	/*
	 * F8770: the three callbacks VOICE_create installs are file-local in
	 * the object and external here, so the two sides necessarily store
	 * different addresses.  Each side's own three are checked by identity
	 * in `check_callbacks` instead.
	 */
	c->cfg.fn_04 = 0;
	c->cfg.fn_08 = 0;
	c->cfg.fn_0c = 0;
}

static void
check_callbacks(struct vce *ours, struct vce *ref, long tag)
{
	diff_eq_int("our cfg.fn_04 is vce_get_sreg",
		    (void *)ours->voice->cfg.fn_04 == (void *)vce_get_sreg, 1,
		    tag);
	diff_eq_int("our cfg.fn_08 is vce_hook_on",
		    (void *)ours->voice->cfg.fn_08 == (void *)vce_hook_on, 1,
		    tag);
	diff_eq_int("our cfg.fn_0c is vce_hook_off",
		    (void *)ours->voice->cfg.fn_0c == (void *)vce_hook_off, 1,
		    tag);
	diff_eq_int("ref cfg.fn_04 is ref_vce_get_sreg",
		    (void *)ref->voice->cfg.fn_04 == (void *)ref_vce_get_sreg,
		    1, tag);
	diff_eq_int("ref cfg.fn_08 is ref_vce_hook_on",
		    (void *)ref->voice->cfg.fn_08 == (void *)ref_vce_hook_on,
		    1, tag);
	diff_eq_int("ref cfg.fn_0c is ref_vce_hook_off",
		    (void *)ref->voice->cfg.fn_0c == (void *)ref_vce_hook_off,
		    1, tag);
	/*
	 * And the rotation still holds through this layer: the S-register
	 * getter is what reaches beepgen's `fn_0124`, the slot beepgen calls
	 * as f(modem, 24).  F8813, seen from one level further out.
	 */
	diff_eq_int("our beepgen fn_0124 is vce_get_sreg",
		    (void *)ours->voice->beepgen->fn_0124 ==
			(void *)vce_get_sreg, 1, tag);
	diff_eq_int("ref beepgen fn_0124 is ref_vce_get_sreg",
		    (void *)ref->voice->beepgen->fn_0124 ==
			(void *)ref_vce_get_sreg, 1, tag);
}

static void
compare_vce(struct vce *ours, struct vce *ref, long tag)
{
	struct vce a, b;
	struct voice_ctx ca, cb;

	diff_eq_int("ours is non-NULL", ours != 0, 1, tag);
	diff_eq_int("ref is non-NULL", ref != 0, 1, tag);
	if (ours == 0 || ref == 0)
		return;

	a = *ours;
	b = *ref;
	normalise_vce(&a);
	normalise_vce(&b);
	diff_eq_obj("vce", struct vce, &a, &b, tag);
	objects_compared++;

	diff_eq_int("voice core presence", ours->voice != 0, ref->voice != 0,
		    tag);
	diff_eq_int("rc_in presence", ours->rc_in != 0, ref->rc_in != 0, tag);
	diff_eq_int("rc_out presence", ours->rc_out != 0, ref->rc_out != 0,
		    tag);

	if (ours->voice != 0 && ref->voice != 0) {
		ca = *ours->voice;
		cb = *ref->voice;
		normalise_ctx(&ca);
		normalise_ctx(&cb);
		diff_eq_obj("voice_ctx", struct voice_ctx, &ca, &cb, tag);
		check_callbacks(ours, ref, tag);
	}
}

/* ------------------------------------------------------------------ */

static const unsigned int rates[] = {
	VCE_RATE_8000, VCE_RATE_9600, VCE_RATE_48000,
	11025,		/* unsupported: neither converter, straight to fail */
	0,		/* and the degenerate one, which is also unsupported */
};
#define NRATES ((int)(sizeof(rates) / sizeof(rates[0])))

static long seen_rate[NRATES];
static long seen_teardown;
static long seen_factors;

static int
t_create(void)
{
	char text_a[TEXTBUF], text_b[TEXTBUF];
	unsigned int lvl;
	int r;

	diff_begin("VOICE_create builds the same object as the blob");

	for (lvl = 0; lvl <= 2; lvl += 2) {
		for (r = 0; r < NRATES; r++) {
			struct vce *a, *b;
			unsigned int rate = rates[r];
			long tag = (long)(lvl * 100 + r);

			harness_alloc_reset();
			set_params();
			fill_info(250);
			set_level(lvl);
			dsplib_debug_capture_reset();
			dsplib_debug_capture_on = 1;

			b = (struct vce *)ref_VOICE_create(&host_modem, rate);
			a = (struct vce *)VOICE_create(&host_modem, rate);

			dsplib_debug_capture_on = 0;
			seen_rate[r]++;

			diff_eq_int("both sides agree on NULL", a == 0,
				    b == 0, tag);

			if (a != 0 && b != 0) {
				compare_vce(a, b, tag);
				diff_eq_int("our allocation size",
					    (long)harness_alloc_reqsize(a),
					    0x1484, tag);
				diff_eq_int("ref allocation size",
					    (long)harness_alloc_reqsize(b),
					    0x1484, tag);

				/*
				 * The block size, straight out of the object's
				 * reciprocal divide by 8000 (F8833).  Read
				 * from OUR object and asserted against the
				 * arithmetic, so a divide by 1000 would fail
				 * here even though both sides would agree.
				 */
				diff_eq_int("block is rate*160/8000",
					    (long)a->block,
					    (long)(rate * 160 / 8000), tag);
				diff_eq_int("out_ring is primed one block "
					    "ahead", (long)a->out_ring.count,
					    (long)a->block, tag);
				diff_eq_int("out_ring starts on the second "
					    "half", (long)a->out_ring.blk,
					    (long)a->block, tag);
				diff_eq_int("in_ring starts empty",
					    (long)a->in_ring.count, 0, tag);
				diff_eq_int("state starts at COMMAND",
					    (long)a->state,
					    VOICE_STATE_COMMAND, tag);
				diff_eq_int("info is the planted block",
					    a->info == &host_info, 1, tag);
				/*
				 * Two converters exactly when the rate is not
				 * the pump's own -- and NEITHER at 8 kHz,
				 * which is what makes D1022 reachable.
				 */
				diff_eq_int("converters iff rate != 8000",
					    a->rc_in != 0,
					    rate != VCE_RATE_8000, tag);

				/*
				 * WHICH CONVERSION EACH CONVERTER IS, and this
				 * one is asserted on OUR side alone because
				 * the blob exports no accessor for it.  It is
				 * not a differential check and does not claim
				 * to be: the mode NUMBERS are the object's
				 * immediates, what they mean comes from this
				 * tree's own `fixedRc_UpFact`/`DownFact`, and
				 * this pins the pairing so the two directions
				 * cannot be swapped.  At 9600 the swap is
				 * caught by the audio in t_voiceproc; at 48000
				 * nothing can run (D1023), so without this
				 * check nothing would see it at all.
				 */
				if (a->rc_in != 0 && a->rc_out != 0) {
					struct rc_state *si =
					    RcFixed_State(a->rc_in);
					struct rc_state *so =
					    RcFixed_State(a->rc_out);
					int down = (int)(rate / 8000);

					diff_eq_int("rc_in reduces to 8 kHz: "
						    "down", (long)si->down,
						    (long)(rate == VCE_RATE_9600
							   ? 6 : down), tag);
					diff_eq_int("rc_in reduces to 8 kHz: up",
						    (long)si->up,
						    (long)(rate == VCE_RATE_9600
							   ? 5 : 1), tag);
					diff_eq_int("rc_out raises from 8 kHz: "
						    "down", (long)so->down,
						    (long)(rate == VCE_RATE_9600
							   ? 5 : 1), tag);
					diff_eq_int("rc_out raises from 8 kHz: "
						    "up", (long)so->up,
						    (long)(rate == VCE_RATE_9600
							   ? 6 : down), tag);
					diff_eq_int("the two are inverses",
						    (long)si->down * so->down,
						    (long)si->up * so->up, tag);
					seen_factors++;
				}

				ref_VOICE_delete(b);
				VOICE_delete(a);
			} else {
				/*
				 * The teardown: allocated, then freed, with
				 * nothing left outstanding.
				 */
				seen_teardown++;
				diff_eq_int("the failed create freed "
					    "everything",
					    (long)harness_alloc.live, 0, tag);
				diff_eq_int("the failed create did allocate",
					    harness_alloc.allocs > 0, 1, tag);
				diff_eq_int("no wild frees",
					    (long)harness_alloc.bad_free, 0,
					    tag);
			}

			diff_eq_int("create/delete balance",
				    (long)harness_alloc.live, 0, tag);

			filter_text(text_a, dsplib_debug_capture_text(0));
			filter_text(text_b, dsplib_debug_capture_text(1));
			diff_eq_int("transcript", strcmp(text_a, text_b) == 0,
				    1, tag);
			/*
			 * Anti-vacuity: the capture buffer is never empty
			 * (the harness writes its own `<< get_param >>`
			 * markers into it), so what is asserted is the
			 * PRINTED line count, which is what the `> 1` gates
			 * actually control.
			 */
			diff_eq_int("printed lines iff level 2",
				    (long)dsplib_debug_capture_lines(1) > 0,
				    lvl > 1, tag);
		}
	}
	set_level(0);
	return diff_end();
}

/* ------------------------------------------------------------------ */

/*
 * `VOICE_command`'s eight arms, counted from the REFERENCE transcript by the
 * one line each prints.  The refusal shares its line with the out-of-range
 * path, which is the point of D1021, so both are counted through it and the
 * two are separated by the OPCODE that was driven.
 */
static const char *const arm_line[8] = {
	"voice: VCE: VOICE_CMD_SET_MODE: COMMAND",
	"voice: VCE: VOICE_CMD_SET_MODE: RX",
	"voice: VCE: VOICE_CMD_SET_MODE: TX",
	"voice: VCE: Unknown command 3",
	"voice: VCE: VOICE_CMD_SET_MODE: SPEAKER",
	"voice: VCE: VOICE_CMD_BEEP,",
	"voice: VCE: VOICE_CMD_DTMF,",
	"voice: VCE: VOICE_CMD_ABORT"
};
static long seen_arm[8];
static long seen_out_of_range;
static long seen_dtmf_floor[2];
static long seen_clears[2];
static long commands_run;

static int
t_command(void)
{
	static const unsigned int cmds[] = {
		0, 1, 2, 3, 4, 5, 6, 7, 8, 9, 100, 0xffffffffu
	};
	static const unsigned int durs[] = { 0, 5, 9, 10, 250, 1000 };
	char text_a[TEXTBUF], text_b[TEXTBUF];
	unsigned int lvl;
	unsigned c, d;

	diff_begin("VOICE_command over every opcode and both duration bands");

	for (lvl = 0; lvl <= 2; lvl += 2) {
		for (d = 0; d < sizeof(durs) / sizeof(durs[0]); d++) {
			for (c = 0; c < sizeof(cmds) / sizeof(cmds[0]); c++) {
				struct vce *a, *b;
				int ra, rb;
				long tag = (long)(lvl * 1000 + d * 100 + c);

				harness_alloc_reset();
				set_params();
				fill_info(durs[d]);
				set_level(0);

				b = (struct vce *)ref_VOICE_create(&host_modem,
								   9600);
				a = (struct vce *)VOICE_create(&host_modem,
							       9600);
				if (a == 0 || b == 0) {
					diff_eq_int("create for the command "
						    "sweep", 0, 1, tag);
					return diff_end();
				}

				/*
				 * A distinct non-zero `host_count` before
				 * every call, because the shared tail clears
				 * it and an arm that skipped the tail would
				 * otherwise be invisible.
				 */
				a->host_count = 0x5a5a + (int)c;
				b->host_count = 0x5a5a + (int)c;

				set_level(lvl);
				dsplib_debug_capture_reset();
				dsplib_debug_capture_on = 1;

				rb = ref_VOICE_command(b, cmds[c]);
				ra = VOICE_command(a, cmds[c]);

				dsplib_debug_capture_on = 0;
				set_level(0);
				commands_run++;

				diff_eq_int("return value", ra, rb, tag);
				compare_vce(a, b, tag);

				if (cmds[c] == 3 || cmds[c] > 7) {
					diff_eq_int("the refused opcodes "
						    "answer -1", ra, -1, tag);
					if (cmds[c] > 7)
						seen_out_of_range++;
				}
				/*
				 * `host_count = 0` IS NOT IN THE SHARED TAIL.
				 * The store at 0x9e0 sits ABOVE the join at
				 * 0x9ec, so only the four state arms -- which
				 * reach 0x9e0 -- clear it; BEEP, DTMF and
				 * ABORT jump straight to 0x9ec and leave it,
				 * and so does every refusal.  Reading the
				 * store as part of the tail is the mistake
				 * this check exists to catch, and it caught
				 * it.  Finding F8837.
				 */
				if (cmds[c] == VOICE_CMD_STATE_COMMAND
				    || cmds[c] == VOICE_CMD_STATE_RX
				    || cmds[c] == VOICE_CMD_STATE_TX
				    || cmds[c] == VOICE_CMD_STATE_SPEAKER) {
					diff_eq_int("a state opcode clears "
						    "host_count",
						    (long)a->host_count, 0,
						    tag);
					seen_clears[0]++;
				} else {
					diff_eq_int("every other opcode "
						    "leaves host_count alone",
						    (long)a->host_count,
						    (long)(0x5a5a + (int)c),
						    tag);
					seen_clears[1]++;
				}

				/*
				 * The DTMF floor, both sides of it: a
				 * duration under 10 ms rounds to nothing and
				 * the argument is floored at 1, while the
				 * debug line still prints the raw quotient.
				 */
				if (cmds[c] == VOICE_CMD_DTMF)
					seen_dtmf_floor[durs[d] / 10 == 0
							? 0 : 1]++;

				filter_text(text_a, dsplib_debug_capture_text(0));
				filter_text(text_b, dsplib_debug_capture_text(1));
				diff_eq_int("transcript",
					    strcmp(text_a, text_b) == 0, 1,
					    tag);

				if (lvl > 1 && cmds[c] < 8
				    && strstr(text_b, arm_line[cmds[c]]) != 0)
					seen_arm[cmds[c]]++;

				ref_VOICE_delete(b);
				VOICE_delete(a);
				diff_eq_int("balance after the command",
					    (long)harness_alloc.live, 0, tag);
			}
		}
	}
	set_level(0);
	return diff_end();
}

static long seen_null_obj;

static int
t_null(void)
{
	unsigned int lvl;

	diff_begin("VOICE_command answers -1 for a NULL handle, silently");

	for (lvl = 0; lvl <= 2; lvl += 2) {
		unsigned int c;

		for (c = 0; c < 10; c++) {
			set_level(lvl);
			dsplib_debug_capture_reset();
			dsplib_debug_capture_on = 1;
			diff_eq_int("ours", VOICE_command(0, c), -1, (long)c);
			diff_eq_int("ref", ref_VOICE_command(0, c), -1,
				    (long)c);
			dsplib_debug_capture_on = 0;
			/*
			 * The NULL test is BEFORE the range check and before
			 * any printf, so nothing is printed at any level.
			 */
			diff_eq_int("nothing printed",
				    (long)dsplib_debug_capture_lines(1), 0,
				    (long)c);
			seen_null_obj++;
		}
	}
	set_level(0);
	return diff_end();
}

/* ------------------------------------------------------------------ */

/*
 * `VOICE_delete` guards every pointer it frees and does NOT guard `obj`, so
 * the four guarded arms are driven by clearing one field at a time on a real
 * object -- which is also the only way to reach the "no converter" arms at a
 * rate that HAS converters.
 */
static long seen_del[4];

static int
t_delete(void)
{
	char text_a[TEXTBUF], text_b[TEXTBUF];
	unsigned int lvl;
	int mask;

	diff_begin("VOICE_delete frees exactly what is present");

	for (lvl = 0; lvl <= 2; lvl += 2) {
		for (mask = 0; mask < 8; mask++) {
			struct vce *a, *b;
			long tag = (long)(lvl * 10 + mask);

			harness_alloc_reset();
			set_params();
			fill_info(250);
			set_level(0);

			b = (struct vce *)ref_VOICE_create(&host_modem, 48000);
			a = (struct vce *)VOICE_create(&host_modem, 48000);
			if (a == 0 || b == 0) {
				diff_eq_int("create for the delete sweep", 0,
					    1, tag);
				return diff_end();
			}

			/*
			 * Drop the sub-objects the mask names -- leaking them
			 * deliberately, which the balance check below accounts
			 * for by looking at `bad_free` rather than `live`.
			 */
			if (mask & 1) {
				a->voice = 0;
				b->voice = 0;
				seen_del[0]++;
			}
			if (mask & 2) {
				a->rc_in = 0;
				b->rc_in = 0;
				seen_del[1]++;
			}
			if (mask & 4) {
				a->rc_out = 0;
				b->rc_out = 0;
				seen_del[2]++;
			}
			if (mask == 0)
				seen_del[3]++;

			set_level(lvl);
			dsplib_debug_capture_reset();
			dsplib_debug_capture_on = 1;
			ref_VOICE_delete(b);
			VOICE_delete(a);
			dsplib_debug_capture_on = 0;
			set_level(0);

			diff_eq_int("no wild or double frees",
				    (long)harness_alloc.bad_free, 0, tag);
			filter_text(text_a, dsplib_debug_capture_text(0));
			filter_text(text_b, dsplib_debug_capture_text(1));
			diff_eq_int("transcript", strcmp(text_a, text_b) == 0,
				    1, tag);
			diff_eq_int("one line iff level 2",
				    (long)dsplib_debug_capture_lines(1) > 0,
				    lvl > 1, tag);
		}
	}
	set_level(0);
	return diff_end();
}

/* ------------------------------------------------------------------ */

static int
t_coverage(void)
{
	int i;

	diff_begin("the sweeps above reached every arm they claim to");

	for (i = 0; i < NRATES; i++)
		diff_eq_int("rate arm %ld was driven", seen_rate[i] > 0, 1,
			    (long)i);
	diff_eq_int("the create teardown was reached", seen_teardown > 0, 1,
		    0);
	diff_eq_int("the converter factors were checked", seen_factors > 0, 1,
		    0);
	for (i = 0; i < 8; i++)
		diff_eq_int("command arm %ld fired", seen_arm[i] > 0, 1,
			    (long)i);
	diff_eq_int("the out-of-range arm fired", seen_out_of_range > 0, 1, 0);
	diff_eq_int("host_count was cleared by some arm", seen_clears[0] > 0,
		    1, 0);
	diff_eq_int("host_count was preserved by some arm", seen_clears[1] > 0,
		    1, 1);
	diff_eq_int("the DTMF floor was taken", seen_dtmf_floor[0] > 0, 1, 0);
	diff_eq_int("the DTMF floor was NOT taken", seen_dtmf_floor[1] > 0, 1,
		    1);
	diff_eq_int("the NULL handle was driven", seen_null_obj > 0, 1, 0);
	for (i = 0; i < 4; i++)
		diff_eq_int("delete arm %ld was driven", seen_del[i] > 0, 1,
			    (long)i);
	diff_eq_int("objects were actually compared", objects_compared > 0, 1,
		    0);
	for (i = 0; i < 3; i++)
		diff_eq_int("transcript filter category %ld fired",
			    filtered_lines[i] > 0, 1, (long)i);

	fprintf(stderr,
		"t_voiceapi: %ld objects compared, %ld commands run, "
		"rates %ld/%ld/%ld/%ld/%ld, teardowns %ld\n",
		objects_compared, commands_run, seen_rate[0], seen_rate[1],
		seen_rate[2], seen_rate[3], seen_rate[4], seen_teardown);
	fprintf(stderr,
		"t_voiceapi: command arms %ld/%ld/%ld/%ld/%ld/%ld/%ld/%ld, "
		"out-of-range %ld, DTMF floor %ld/%ld, NULL %ld, "
		"delete arms %ld/%ld/%ld/%ld, filtered %ld\n",
		seen_arm[0], seen_arm[1], seen_arm[2], seen_arm[3],
		seen_arm[4], seen_arm[5], seen_arm[6], seen_arm[7],
		seen_out_of_range, seen_dtmf_floor[0], seen_dtmf_floor[1],
		seen_null_obj, seen_del[0], seen_del[1], seen_del[2],
		seen_del[3], filtered_lines[0] + filtered_lines[1]
			     + filtered_lines[2]);
	return diff_end();
}

int
main(void)
{
	int failed = 0;

	failed |= t_create();
	failed |= t_command();
	failed |= t_null();
	failed |= t_delete();
	failed |= t_coverage();
	return failed;
}

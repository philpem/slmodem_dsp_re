/*
 * t_voicesvc.c -- differential test of the voice service core:
 * `voice_create` (0xac210), `voice_delete` (0xac150), `voice_command`
 * (0xac4a0) and `voice_modem` (0xac800).
 *
 * NOTHING HERE IS HAND-PLANTED, and that is deliberate.  D955/F8587's rule
 * is that a fixture must plant every field a callee uses as a SUBSCRIPT and
 * that a blob-against-blob dry run cannot catch a missed one, because both
 * sides read the same wild index and agree.  The way out of that class of
 * bug entirely is to let the constructor under test build the object: every
 * `struct voice_ctx` below comes from `voice_create` or `ref_voice_create`,
 * so there is no field this test forgot to fill.  The one exception is
 * `t_delete_shapes`, which NULLs pointers in a constructed graph after
 * releasing what they pointed at -- subtraction from a complete object,
 * never addition to an empty one.
 *
 * THE HOST SIDE IS OURS AND IS SHARED.  `modem`, the S-register getter and
 * the two hooks are this file's, so both sides see identical callbacks and
 * their addresses compare equal.  That is what lets the rotation
 * `voice_create` performs on its config (finding F8813) be asserted directly:
 * the beep generator's three callback slots hold values this file chose, and
 * which slot each landed in is the claim under test.
 *
 * WHAT CANNOT BE COMPARED BYTE FOR BYTE is any field holding a pointer into
 * one side's own graph -- the five sub-objects, and `handler`, which is
 * `voice_online` on our side and `ref_voice_online` on the blob's.  Those are
 * zeroed in a COPY before `diff_eq_obj` sees it, and asserted separately, so
 * the object comparison stays a `diff_eq_obj` over the real type rather than
 * an open-coded byte loop with a skip list.
 *
 * MULTI-BLOCK, because F8790: a swapped smoothing weight -- or here a
 * detector counter that advances on the wrong subscript -- is invisible to a
 * one-block fixture and to every codegen check.  `t_modem` runs 48 blocks per
 * trial and compares after each one.
 */

#include <math.h>
#include <stdio.h>
#include <string.h>

#include "harness.h"
#include "dsplib/beepgen.h"
#include "dsplib/cadence.h"
#include "dsplib/debug.h"
#include "dsplib/detector.h"
#include "dsplib/fdspkrnl.h"
#include "dsplib/fifo8.h"
#include "dsplib/modem_params.h"
#include "dsplib/silence.h"
#include "dsplib/sysdep.h"
#include "dsplib/toneiir.h"
#include "dsplib/voice.h"

#define TWOPI	6.283185307179586

extern unsigned int ref_dsplibs_debug_level;
extern int ref_bInternalBeepInProgress;
/* FILE-LOCAL in the object; static in Fdspkrnl.c, declared here for the test. */
extern int bInternalBeepInProgress;
/* The object's own tone table -- the four frequencies, in `tone[]`'s order. */
extern float ref_tone[4];

extern struct voice_ctx *ref_voice_create(const struct voice_config *cfg);
extern void ref_voice_delete(struct voice_ctx *v);
extern int ref_voice_command(struct voice_ctx *v, int cmd, int *arg);
extern int ref_voice_modem(struct voice_ctx *v, short *rx_lin, float *rx_flt,
			   float *tx_flt, short *tx_lin,
			   unsigned short *hostcount,
			   unsigned short *countp);

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

extern void ref_beepgen_delete(struct beepgen *bg);
extern void ref_detector_delete(struct detector *d);
extern void ref_FIFO8_delete(struct fifo8 *f);
extern void ref_silence_delete(struct silence *s);
extern void ref_FDSP_DP_Delete(struct fdsp_kernel *k);
extern void ref_detector_set_enable(struct detector *d, short enable);

extern int dsplib_debug_capture_on;
void dsplib_debug_capture_reset(void);
unsigned dsplib_debug_capture_lines(int side);
const char *dsplib_debug_capture_text(int side);

/* ------------------------------------------------------------------ */
/* The host side.                                                     */

static int host_modem;			/* an opaque handle, ours          */
static long sreg_calls, hook08_calls, hook0c_calls;
static long sreg_last;

static unsigned int
host_get_sreg(void *modem, int num)
{
	sreg_calls++;
	sreg_last = num;
	if (modem != (void *)&host_modem)
		return 0;
	/*
	 * Deterministic and non-trivial: every S-register answers a distinct
	 * value, so a callee that asked for the wrong one gets a different
	 * number rather than the same plausible zero.
	 */
	return (unsigned int)(num * 3 + 1);
}

static void
host_fn_08(void *modem)
{
	(void)modem;
	hook08_calls++;
}

static void
host_fn_0c(void *modem)
{
	(void)modem;
	hook0c_calls++;
}

static void
fill_cfg(struct voice_config *c)
{
	c->modem = &host_modem;
	c->fn_04 = host_get_sreg;
	c->fn_08 = host_fn_08;
	c->fn_0c = host_fn_0c;
}

static void
set_level(unsigned int lvl)
{
	dsplibs_debug_level = lvl;
	ref_dsplibs_debug_level = lvl;
}

/*
 * THE COUNTRY PARAMETERS `cadence_create` CONSULTS, AND WHY THEY MUST BE SET.
 *
 * `voice_create` reaches `cadence_create` through `detector_create`, and that
 * function turns `GetDialToneDetectionThreshold` into a SUBSCRIPT --
 * `Get_Detection_Threshold_Table(level_fix)`.  The harness's unforced answer
 * is `0x5A000000 + param * 7`, whose low sixteen bits are a wild index, and
 * the two sides then read different entries: `cadence.threshold` came out 7
 * on ours and 0 on the blob's until this existed.  That is D955/F8587 in its
 * exact shape -- an unplanted SUBSCRIPT, not an unplanted dereference -- and
 * it was found here by comparing the cadence objects word for word, which is
 * why `compare_graph` does that rather than trusting the context alone.
 *
 * The values are t_detector's, so the two tests drive the same country.
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

static long filtered_lines;

/*
 * ONE DEBUG LINE ON THIS PATH CANNOT BE COMPARED, and it is the object's own
 * doing rather than the fixture's.  `STRM_VCE_GetFDSPEnvironmentalParams` is
 * a setter that prints its two output words BEFORE writing them (deviation
 * D991), so "voice: StrmVCE old:" reports whatever was on that side's stack
 * -- ours says -31248/2412 where the blob says 2052/-24112, and both are
 * correct.  The line is dropped from both transcripts and counted, so a
 * filter that silently matched everything would show up as a line count of
 * zero rather than as a clean pass.  Every other line is compared in full.
 */
#define DROP_PREFIX	"voice: StrmVCE old:"
#define TEXTBUF		8192

static void
filter_text(char *dst, const char *src)
{
	unsigned n = 0;

	while (*src != '\0') {
		const char *nl = strchr(src, '\n');
		unsigned len = (nl != 0) ? (unsigned)(nl - src) + 1
					 : (unsigned)strlen(src);

		if (strncmp(src, DROP_PREFIX, sizeof(DROP_PREFIX) - 1) == 0) {
			filtered_lines++;
		} else if (n + len < TEXTBUF) {
			memcpy(dst + n, src, len);
			n += len;
		}
		src += len;
	}
	dst[n] = '\0';
}

/* ------------------------------------------------------------------ */
/* Comparing a constructed context.                                   */

/*
 * Zero every field that necessarily holds a different value on the two sides
 * -- the five sub-object pointers, the host block (identical, but zeroed on
 * both so the comparison says nothing about it either way is not needed:
 * it IS identical, so it stays) and `handler`.
 */
static void
normalise_ctx(struct voice_ctx *c)
{
	c->beepgen = 0;
	c->detector = 0;
	c->fifo = 0;
	c->silence = 0;
	c->dp = 0;
	c->handler = 0;
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

/*
 * One cadence, word by word, skipping the pointers (each side's own) and
 * `int_27c`, which takes an uninitialised stack word at construction and is
 * read by nothing -- deviation D1000.  The same skip list t_detector uses,
 * and for the same reason.
 */
static void
cmp_cadence(const struct cadence *ours, const struct cadence *ref, long tag)
{
	int i;

	for (i = 0; i < (int)(sizeof(struct cadence) / sizeof(int)); i++) {
		const int *pa = (const int *)ref, *pb = (const int *)ours;
		size_t off = (size_t)i * sizeof(int);

		if (off == offsetof(struct cadence, filter)
		    || off == offsetof(struct cadence, sel_a)
		    || off == offsetof(struct cadence, sel_b)
		    || off == offsetof(struct cadence, sel_scales)
		    || off == offsetof(struct cadence, name)
		    || off == offsetof(struct cadence, modem)
		    || off == offsetof(struct cadence, int_27c))
			continue;
		diff_eq_int("cadence word at +0x%lx", pb[i], pa[i],
			    tag * 4096 + (long)off);
	}
}

/*
 * Compare two constructed graphs as far as they can be compared: the context
 * itself, the beep generator whole (its only pointers are the host's, so they
 * ARE comparable), the detector with its own sub-pointers normalised, the
 * silence detector whole, and the FIFO minus its buffer pointer.
 */
static void
compare_graph(const char *what, struct voice_ctx *ours, struct voice_ctx *ref,
	      long tag)
{
	struct voice_ctx ca, cb;
	struct detector da, db;
	struct fifo8 fa, fb;

	(void)what;
	diff_eq_int("ours is non-NULL", ours != 0, 1, tag);
	diff_eq_int("ref is non-NULL", ref != 0, 1, tag);
	if (ours == 0 || ref == 0)
		return;

	ca = *ours;
	cb = *ref;
	normalise_ctx(&ca);
	normalise_ctx(&cb);
	diff_eq_obj("voice_ctx", struct voice_ctx, &ca, &cb, tag);

	/* Each sub-object is present on both sides, or absent on both. */
	diff_eq_int("beepgen presence", ours->beepgen != 0, ref->beepgen != 0,
		    tag);
	diff_eq_int("detector presence", ours->detector != 0,
		    ref->detector != 0, tag);
	diff_eq_int("fifo presence", ours->fifo != 0, ref->fifo != 0, tag);
	diff_eq_int("silence presence", ours->silence != 0, ref->silence != 0,
		    tag);
	diff_eq_int("dp presence", ours->dp != 0, ref->dp != 0, tag);

	if (ours->beepgen != 0 && ref->beepgen != 0)
		diff_eq_obj("beepgen", struct beepgen, ours->beepgen,
			    ref->beepgen, tag);

	if (ours->detector != 0 && ref->detector != 0) {
		da = *ours->detector;
		db = *ref->detector;
		normalise_detector(&da);
		normalise_detector(&db);
		diff_eq_obj("detector", struct detector, &da, &db, tag);
		cmp_cadence(ours->detector->cadence_busy,
			    ref->detector->cadence_busy, tag * 2);
		cmp_cadence(ours->detector->cadence_dial,
			    ref->detector->cadence_dial, tag * 2 + 1);
	}

	if (ours->silence != 0 && ref->silence != 0)
		diff_eq_obj("silence", struct silence, ours->silence,
			    ref->silence, tag);

	if (ours->fifo != 0 && ref->fifo != 0) {
		fa = *ours->fifo;
		fb = *ref->fifo;
		fa.buf = 0;
		fb.buf = 0;
		diff_eq_obj("fifo8", struct fifo8, &fa, &fb, tag);
		diff_eq_int("fifo buffer present", ours->fifo->buf != 0,
			    ref->fifo->buf != 0, tag);
	}
}

/* ------------------------------------------------------------------ */

static long n_create, seen_level[2], seen_null_cfg, seen_delete_line;

static int
t_create(void)
{
	struct voice_config cfg;
	struct voice_ctx *a, *b;
	char text_a[TEXTBUF], text_b[TEXTBUF];
	unsigned int lvl;

	diff_begin("voice_create builds the same object as the blob");
	fill_cfg(&cfg);

	for (lvl = 0; lvl <= 2; lvl += 2) {
		harness_alloc_reset();
		set_params();
		set_level(lvl);
		dsplib_debug_capture_reset();
		dsplib_debug_capture_on = 1;
		sreg_calls = 0;

		b = ref_voice_create(&cfg);
		a = voice_create(&cfg);

		dsplib_debug_capture_on = 0;
		seen_level[lvl ? 1 : 0]++;

		compare_graph("create", a, b, (long)lvl);
		if (a == 0 || b == 0)
			return diff_end();

		/* 0x7dc is the object's own allocation size. */
		diff_eq_int("our allocation size", harness_alloc_reqsize(a),
			    0x7dc, (long)lvl);
		diff_eq_int("ref allocation size", harness_alloc_reqsize(b),
			    0x7dc, (long)lvl);
		diff_eq_int("sizeof(struct voice_ctx)",
			    (long)sizeof(struct voice_ctx), 0x7dc, (long)lvl);

		/* The handler each side installed is its own voice_online. */
		diff_eq_int("our handler is voice_online",
			    a->handler == voice_online, 1, (long)lvl);
		diff_eq_int("ref handler is ref_voice_online",
			    b->handler == ref_voice_online, 1, (long)lvl);

		/*
		 * THE ROTATION (F8813).  The beep generator's three callback
		 * slots must hold the voice config's words in the rotated
		 * order, and the S-register getter must be the one that
		 * reached `fn_0124` -- the slot beepgen calls as f(modem, 24).
		 */
		diff_eq_int("beepgen fn_011c is cfg.fn_08",
			    a->beepgen->fn_011c == host_fn_08, 1, (long)lvl);
		diff_eq_int("beepgen hook_on_proc is cfg.fn_0c",
			    a->beepgen->hook_on_proc == host_fn_0c, 1,
			    (long)lvl);
		diff_eq_int("beepgen fn_0124 is cfg.fn_04",
			    (void *)a->beepgen->fn_0124 ==
				(void *)host_get_sreg, 1, (long)lvl);
		diff_eq_int("the blob rotates identically",
			    b->beepgen->fn_011c == host_fn_08 &&
				b->beepgen->hook_on_proc == host_fn_0c &&
				(void *)b->beepgen->fn_0124 ==
				    (void *)host_get_sreg,
			    1, (long)lvl);
		diff_eq_int("beepgen modem is the host handle",
			    a->beepgen->modem == (void *)&host_modem, 1,
			    (long)lvl);

		/*
		 * And the getter reached `detector_create` and
		 * `silence_create`: 73 * 3 + 1 = 220, times 50/4 is 2750.
		 */
		diff_eq_int("detector took the S-register getter",
			    a->detector->dialtone_detect_delay, 220 * 50 / 4,
			    (long)lvl);
		diff_eq_int("silence took the S-register getter",
			    (void *)a->silence->query == (void *)host_get_sreg,
			    1, (long)lvl);
		diff_eq_int("the getter was actually called", sreg_calls > 0, 1,
			    (long)lvl);

		/*
		 * The two transcripts, line count and text.  The text is
		 * comparable here for the same reason it is in `one_command`
		 * -- once the "voice: StrmVCE old:" line is filtered out,
		 * nothing on this path prints a value that differs between
		 * the sides.  `beepgen_create`'s "%X  %X" prints two of the
		 * host callbacks, which are this file's and so equal, and
		 * `FDSP_DP_Create`'s "ver 120 sRxSamplesDelay %d
		 * ,sTxSamplesDelay %d" prints 51 and 369 -- which is what
		 * catches the two echo delays being handed over swapped.
		 */
		filter_text(text_a, dsplib_debug_capture_text(0));
		filter_text(text_b, dsplib_debug_capture_text(1));
		diff_eq_int("debug lines",
			    (long)dsplib_debug_capture_lines(0),
			    (long)dsplib_debug_capture_lines(1), (long)lvl);
		diff_eq_int("debug text", strcmp(text_a, text_b), 0,
			    (long)lvl);
		diff_eq_int("level 2 printed, level 0 did not",
			    dsplib_debug_capture_lines(0) != 0, lvl > 1 ? 1 : 0,
			    (long)lvl);

		/*
		 * `voice_delete`'s line prints the CONTEXT POINTER, which is
		 * necessarily different on the two sides -- so the two
		 * transcripts cannot be compared and each side is checked
		 * against its OWN pointer instead.  Without this the
		 * argument is unobserved and could be anything.
		 */
		dsplib_debug_capture_reset();
		dsplib_debug_capture_on = 1;
		ref_voice_delete(b);
		voice_delete(a);
		dsplib_debug_capture_on = 0;
		if (lvl > 1) {
			char want_a[64], want_b[64];

			sprintf(want_a, "voice delete %lX\n",
				(unsigned long)a);
			sprintf(want_b, "voice delete %lX\n",
				(unsigned long)b);
			diff_eq_int("our delete line names our context",
				    strstr(dsplib_debug_capture_text(0),
					   want_a) != 0, 1, (long)lvl);
			diff_eq_int("the blob's names the blob's",
				    strstr(dsplib_debug_capture_text(1),
					   want_b) != 0, 1, (long)lvl);
			seen_delete_line++;
		}
		diff_eq_int("delete's debug lines",
			    (long)dsplib_debug_capture_lines(0),
			    (long)dsplib_debug_capture_lines(1), (long)lvl);

		diff_eq_int("the arena is empty after delete",
			    harness_alloc.live, 0, (long)lvl);
		diff_eq_int("no bad frees", harness_alloc.bad_free, 0,
			    (long)lvl);
		n_create++;
	}

	/* A NULL config is refused before anything is allocated. */
	set_level(0);
	harness_alloc_reset();
	diff_eq_int("NULL cfg, ours", voice_create(0) == 0, 1, 0);
	diff_eq_int("NULL cfg, ref", ref_voice_create(0) == 0, 1, 0);
	diff_eq_int("NULL cfg allocated nothing", harness_alloc.allocs, 0, 0);
	seen_null_cfg++;

	return diff_end();
}

/* ------------------------------------------------------------------ */

static long n_delete_shapes, delete_arm[5][2];

/*
 * Release the sub-objects NOT named by `keep` and NULL their slots, so
 * `voice_delete` meets every combination of its five guards with an arena
 * that still balances.  `ref` selects which side's destructors to use, so the
 * blob's `pGlobalFDSPObj` is cleared by the blob's `FDSP_DP_Delete`.
 */
static void
thin(struct voice_ctx *v, unsigned keep, int ref)
{
	if (!(keep & 1)) {
		if (ref)
			ref_beepgen_delete(v->beepgen);
		else
			beepgen_delete(v->beepgen);
		v->beepgen = 0;
	}
	if (!(keep & 2)) {
		if (ref)
			ref_detector_delete(v->detector);
		else
			detector_delete(v->detector);
		v->detector = 0;
	}
	if (!(keep & 4)) {
		if (ref)
			ref_FIFO8_delete(v->fifo);
		else
			FIFO8_delete(v->fifo);
		v->fifo = 0;
	}
	if (!(keep & 8)) {
		if (ref)
			ref_silence_delete(v->silence);
		else
			silence_delete(v->silence);
		v->silence = 0;
	}
	if (!(keep & 16)) {
		if (ref)
			ref_FDSP_DP_Delete((struct fdsp_kernel *)v->dp);
		else
			FDSP_DP_Delete((struct fdsp_kernel *)v->dp);
		v->dp = 0;
	}
}

static int
t_delete_shapes(void)
{
	struct voice_config cfg;
	struct voice_ctx *a, *b;
	unsigned keep;
	int i;

	diff_begin("voice_delete over all 32 pointer shapes");
	fill_cfg(&cfg);
	set_level(0);

	for (keep = 0; keep < 32; keep++) {
		harness_alloc_reset();
		set_params();
		b = ref_voice_create(&cfg);
		a = voice_create(&cfg);
		if (a == 0 || b == 0) {
			diff_eq_int("both built", 0, 1, (long)keep);
			return diff_end();
		}
		thin(a, keep, 0);
		thin(b, keep, 1);

		for (i = 0; i < 5; i++)
			delete_arm[i][(keep >> i) & 1]++;

		ref_voice_delete(b);
		voice_delete(a);

		diff_eq_int("the arena is empty", harness_alloc.live, 0,
			    (long)keep);
		diff_eq_int("no bad frees", harness_alloc.bad_free, 0,
			    (long)keep);
		diff_eq_int("no NULL frees", harness_alloc.free_null, 0,
			    (long)keep);
		n_delete_shapes++;
	}
	return diff_end();
}

/* ------------------------------------------------------------------ */

static long n_command, cmd_arm[13], cmd_gate[2], cmd_ret[2];

static int
one_command(int cmd, int mode, int a0, int a1, int a2, int bd, long tag)
{
	struct voice_config cfg;
	struct voice_ctx *a, *b;
	char text_a[TEXTBUF], text_b[TEXTBUF];
	int arg_a[3], arg_b[3];
	int ra, rb;

	fill_cfg(&cfg);
	harness_alloc_reset();
	set_params();
	set_level(2);
	b = ref_voice_create(&cfg);
	a = voice_create(&cfg);
	if (a == 0 || b == 0) {
		diff_eq_int("both built", 0, 1, tag);
		return 0;
	}
	a->mode = mode;
	b->mode = mode;
	/*
	 * `voice_create` leaves `beep_done` at 1, and three arms write 1 into
	 * it -- so with the constructor's value left alone, deleting those
	 * stores changes nothing anyone can see.  Every trial is run at both
	 * values.
	 */
	a->beep_done = bd;
	b->beep_done = bd;

	arg_a[0] = a0;
	arg_a[1] = a1;
	arg_a[2] = a2;
	memcpy(arg_b, arg_a, sizeof(arg_a));

	bInternalBeepInProgress = 0;
	ref_bInternalBeepInProgress = 0;

	dsplib_debug_capture_reset();
	dsplib_debug_capture_on = 1;
	rb = ref_voice_command(b, cmd, arg_b);
	ra = voice_command(a, cmd, arg_a);
	dsplib_debug_capture_on = 0;

	diff_eq_int("voice_command return", ra, rb, tag);
	diff_eq_int("the arg array was not written",
		    memcmp(arg_a, arg_b, sizeof(arg_a)), 0, tag);
	diff_eq_int("debug lines", (long)dsplib_debug_capture_lines(0),
		    (long)dsplib_debug_capture_lines(1), tag);
	/*
	 * Every format string on these paths prints integers or nothing, so
	 * unlike create/delete the TEXT is comparable and is compared.
	 */
	filter_text(text_a, dsplib_debug_capture_text(0));
	filter_text(text_b, dsplib_debug_capture_text(1));
	diff_eq_int("debug text", strcmp(text_a, text_b), 0, tag);
	diff_eq_int("beep-in-progress flag", bInternalBeepInProgress,
		    ref_bInternalBeepInProgress, tag);

	compare_graph("command", a, b, tag);

	if (ra == 0)
		cmd_ret[0]++;
	else if (ra == 7)
		cmd_ret[1]++;

	ref_voice_delete(b);
	voice_delete(a);
	diff_eq_int("the arena is empty", harness_alloc.live, 0, tag);
	diff_eq_int("no bad frees", harness_alloc.bad_free, 0, tag);
	n_command++;
	return ra;
}

static int
t_command(void)
{
	int cmd, mode;
	long tag = 0;

	diff_begin("voice_command, every opcode and both gate outcomes");

	for (cmd = -1; cmd <= 12; cmd++) {
		/*
		 * Modes 2 and 3 pass the beep/DTMF gate, 0 and 5 fail it, so
		 * both outcomes are driven for both gated arms.
		 */
		static const int modes[4] = { 0, 2, 3, 5 };
		int m;

		for (m = 0; m < 4; m++) {
			int bd;

			mode = modes[m];
			for (bd = 0; bd < 2; bd++) {
				one_command(cmd, mode, 1, 2, 3, bd, tag++);
				if (cmd >= 0 && cmd <= 12)
					cmd_arm[cmd]++;
				if (cmd == VOICE_BEEP_COMMAND ||
				    cmd == VOICE_DTMF_COMMAND)
					cmd_gate[(mode == 2 || mode == 3)
						 ? 1 : 0]++;
			}
		}
	}

	/*
	 * VOICE_SET_MODE_COMMAND's four arms and its default, each with the
	 * argument that selects it.  The default (4) must still raise
	 * `beep_done`, which `compare_graph` sees in the context.
	 */
	for (mode = 0; mode <= 4; mode++) {
		one_command(VOICE_SET_MODE_COMMAND, VOICE_MODE_ONLINE, mode,
			    0, 0, 0, 1000 + mode);
		one_command(VOICE_SET_MODE_COMMAND, VOICE_MODE_ONLINE, mode,
			    0, 0, 1, 1010 + mode);
	}

	/*
	 * The three-byte mask word, with the top byte's high bit set: the
	 * object's `sar $0x10` then `and $0xff` and an unsigned shift differ
	 * exactly here, and only here.
	 */
	one_command(VOICE_DETECTOR_ENABLE_COMMAND, VOICE_MODE_ONLINE,
		    (int)0x80402010, 0, 0, 0, 1100);
	one_command(VOICE_DETECTOR_ENABLE_COMMAND, VOICE_MODE_ONLINE,
		    (int)0xffffffff, 0, 0, 0, 1101);
	one_command(VOICE_DETECTOR_ENABLE_COMMAND, VOICE_MODE_ONLINE,
		    0x00123456, 0, 0, 0, 1102);

	/*
	 * Negative and large arguments for the three that truncate to 16 and
	 * the one that does not.  0x12345678 into VLS is what separates
	 * `out_format = arg[0]` from `out_format = (short)arg[0]`: every
	 * value this test used before it fitted in a short, so the narrowing
	 * was unobservable.
	 */
	one_command(VOICE_PLAYBACK_VOLUME_COMMAND, VOICE_MODE_ONLINE,
		    -12345, 0, 0, 0, 1200);
	one_command(VOICE_TIME_MARK_COMMAND, VOICE_MODE_ONLINE, 0x1234abcd, 0,
		    0, 0, 1201);
	one_command(VOICE_VLS_COMMAND, VOICE_MODE_ONLINE, -7, 0, 0, 0, 1202);
	one_command(VOICE_VLS_COMMAND, VOICE_MODE_ONLINE, 0x12345678, 0, 0, 0,
		    1203);
	one_command(VOICE_PLAYBACK_VOLUME_COMMAND, VOICE_MODE_ONLINE,
		    0x7ead1234, 0, 0, 0, 1204);
	one_command(VOICE_TIME_MARK_COMMAND, VOICE_MODE_ONLINE, -3, 0, 0, 0,
		    1205);

	/* Far out of range, both signs. */
	one_command(11, VOICE_MODE_ONLINE, 0, 0, 0, 0, 1300);
	one_command(-100, VOICE_MODE_ONLINE, 0, 0, 0, 0, 1301);
	one_command(0x7fffffff, VOICE_MODE_ONLINE, 0, 0, 0, 0, 1302);

	return diff_end();
}

/* ------------------------------------------------------------------ */

#define BLK	80
#define TXBYTES	1024

static long n_modem, modem_status[4];	/* returns 10, 11, 12, handler's   */
static long modem_detlen[2];		/* the detector emitted / did not  */
static long modem_mode[2];		/* in-stream arm / status arm      */
static long modem_dle[2];

/* What the installed handler saw, one record per side. */
struct hcall {
	long calls;
	long tx_off;			/* tx_lin argument, in bytes       */
	long countp_is_local;
	unsigned short countp_val;
	long saw_ctx;
};

static struct hcall hlog[2];
static int cur_side;
static int handler_ret;
static unsigned short *caller_countp;
static unsigned char *tx_base;
static struct voice_ctx *cur_ctx;

static int
probe_handler(struct voice_ctx *v, short *rx_lin, float *rx_flt, float *tx_flt,
	      short *tx_lin, unsigned short *hostcount, unsigned short *countp)
{
	struct hcall *h = &hlog[cur_side];

	(void)rx_lin;
	(void)rx_flt;
	(void)tx_flt;
	(void)hostcount;
	h->calls++;
	h->tx_off = (long)((unsigned char *)tx_lin - tx_base);
	h->countp_is_local = countp != caller_countp;
	h->countp_val = *countp;
	h->saw_ctx = v == cur_ctx;
	return handler_ret;
}

/*
 * FORCING A CADENCE, so the status codes 1 and 2 are reachable.
 *
 * Copied from t_detector, which established it: a passthrough IIR makes the
 * cadence's `toneiir` report TONEIIR_PRESENT for any non-silent input, and
 * `continuous` makes `cadence_progress` answer CADENCE_DETECTED.  Both sides
 * are pointed at THESE arrays, so the filter is identical on both; the point
 * is to reach `voice_modem`'s 10 and 11 arms deterministically, not to test
 * the cadence detector.
 */
static const short pass_b[IIR_FILTER_COEFF] = {
	IIR_FILTER_ONE, 0, 0, IIR_FILTER_ONE, 0, 0,
	IIR_FILTER_ONE, 0, 0, IIR_FILTER_ONE, 0, 0
};
static const short pass_a[IIR_FILTER_COEFF] = { 0 };
static const short pass_scales[IIR_FILTER_SCALES] = { 0 };

static void
force_cadence(struct cadence *c)
{
	c->continuous = 1;
	c->filter->cfg.a = pass_a;
	c->filter->cfg.b = pass_b;
	c->filter->cfg.scales = pass_scales;
	c->filter->cfg.interval = 16;
	c->filter->cfg.threshold = 0;
	c->filter->cfg.stability = 0;
	c->filter->need = 0;
	c->filter->n = 0;
	c->filter->env_band = 0;
	c->filter->env_in = 0;
	c->filter->env_prev = 0;
	c->filter->total = 0;
	c->filter->run = 0;
}

/*
 * One trial: build both sides, force a detector enable mask and a mode, then
 * run `nblk` blocks through each and compare everything after each one.
 *
 * `tone_idx` selects the input: 0..3 emits that entry of the object's own
 * `tone[]` table, which is what makes a tone counter CLIMB -- `TONE_detect`
 * answers 0 when the tone is present and `detector_progress` integrates the
 * zeros (t_detector's finding, restated because the sense is the opposite of
 * the obvious one).  -1 emits a deterministic broadband ramp instead, which
 * keeps every tone counter at zero and drives only the cadence and DTMF arms.
 */
static int
modem_trial(int mode, short enable, int tone_idx, int nblk, int dle_at,
	    int dle_which, int busy_on, int dial_on, long tag)
{
	int last = -1;
	struct voice_config cfg;
	struct voice_ctx *a, *b;
	float txf[BLK], rxf[BLK];
	short rxl[BLK];
	unsigned char txa[TXBYTES], txb[TXBYTES];
	unsigned short hca, hcb, cnta, cntb;
	double phase = 0.0;
	int blk, i;

	fill_cfg(&cfg);
	harness_alloc_reset();
	set_params();
	set_level(0);
	b = ref_voice_create(&cfg);
	a = voice_create(&cfg);
	if (a == 0 || b == 0) {
		diff_eq_int("both built", 0, 1, tag);
		return -1;
	}

	a->mode = mode;
	b->mode = mode;
	a->handler = probe_handler;
	b->handler = probe_handler;
	detector_set_enable(a->detector, enable);
	ref_detector_set_enable(b->detector, enable);
	if (busy_on) {
		force_cadence(a->detector->cadence_busy);
		force_cadence(b->detector->cadence_busy);
	}
	if (dial_on) {
		force_cadence(a->detector->cadence_dial);
		force_cadence(b->detector->cadence_dial);
	}
	modem_mode[mode == VOICE_MODE_DUPLEX ? 1 : 0]++;

	for (blk = 0; blk < nblk; blk++) {
		int ra, rb;
		int dle_here = (dle_at >= 0 && blk == dle_at);

		for (i = 0; i < BLK; i++) {
			int k = (blk * BLK + i) & 63;

			if (tone_idx >= 0) {
				txf[i] = (float)(0.8 * sin(phase));
				phase += TWOPI * (double)ref_tone[tone_idx]
				    / 8000.0;
			} else {
				txf[i] = (float)(k - 32) * (1.0f / 512.0f);
			}
			rxf[i] = (float)(31 - k) * (1.0f / 512.0f);
			rxl[i] = (short)(k * 37 - 1000);
		}
		memset(txa, 0x5a, sizeof(txa));
		memset(txb, 0x5a, sizeof(txb));
		hca = hcb = 200;
		cnta = cntb = BLK;
		handler_ret = 42;

		if (dle_here) {
			/*
			 * ONE FLAG AT A TIME, because the object's test is an
			 * OR: setting both makes `||` and `&&` agree and the
			 * difference between them unobservable.  `dle_which`
			 * is 1 for <DLE><ETX>, 2 for <DLE><CAN>, 3 for both.
			 */
			a->dle_etx = b->dle_etx = (dle_which & 1) ? 1 : 0;
			a->dle_can = b->dle_can = (dle_which & 2) ? 1 : 0;
			modem_dle[1]++;
		} else {
			modem_dle[0]++;
		}

		memset(&hlog[0], 0, sizeof(hlog[0]));
		memset(&hlog[1], 0, sizeof(hlog[1]));

		cur_side = 1;
		caller_countp = &cntb;
		tx_base = txb;
		cur_ctx = b;
		rb = ref_voice_modem(b, rxl, rxf, txf, (short *)txb, &hcb,
				     &cntb);

		cur_side = 0;
		caller_countp = &cnta;
		tx_base = txa;
		cur_ctx = a;
		ra = voice_modem(a, rxl, rxf, txf, (short *)txa, &hca, &cnta);

		diff_eq_int("voice_modem return", ra, rb, tag * 1000 + blk);
		diff_eq_int("*countp", cnta, cntb, tag * 1000 + blk);
		diff_eq_int("*hostcount", hca, hcb, tag * 1000 + blk);
		diff_eq_int("the tx buffer",
			    memcmp(txa, txb, sizeof(txa)), 0,
			    tag * 1000 + blk);

		diff_eq_int("handler calls", hlog[0].calls, hlog[1].calls,
			    tag * 1000 + blk);
		diff_eq_int("the probe handler ran once", hlog[0].calls, 1,
			    tag * 1000 + blk);
		diff_eq_int("handler's tx_lin offset", hlog[0].tx_off,
			    hlog[1].tx_off, tag * 1000 + blk);
		diff_eq_int("handler's countp is a local",
			    hlog[0].countp_is_local, 1, tag * 1000 + blk);
		diff_eq_int("the blob's countp is a local too",
			    hlog[1].countp_is_local, 1, tag * 1000 + blk);
		diff_eq_int("handler's *countp", hlog[0].countp_val,
			    hlog[1].countp_val, tag * 1000 + blk);
		diff_eq_int("handler's *countp is the block count",
			    hlog[0].countp_val, BLK, tag * 1000 + blk);
		diff_eq_int("handler got the context", hlog[0].saw_ctx, 1,
			    tag * 1000 + blk);

		/*
		 * THE DLE BLOCK REINSTALLS `voice_online`, so the probe is
		 * only there for one more block unless it is put back.  That
		 * substitution is the observable half of `voice_set_online`
		 * being called at all, so it is asserted here and then undone.
		 */
		diff_eq_int("our handler after the block",
			    a->handler == (dle_here ? voice_online
						    : probe_handler),
			    1, tag * 1000 + blk);
		diff_eq_int("the blob's handler after the block",
			    b->handler == (dle_here ? ref_voice_online
						    : probe_handler),
			    1, tag * 1000 + blk);
		a->handler = probe_handler;
		b->handler = probe_handler;

		/*
		 * The detector's byte count is exactly the offset the handler
		 * was given, and exactly what `*countp` gained.
		 */
		diff_eq_int("countp advanced by the detector's bytes",
			    (long)cnta, (long)(BLK + hlog[0].tx_off),
			    tag * 1000 + blk);
		modem_detlen[hlog[0].tx_off != 0 ? 1 : 0]++;

		if (ra == 10)
			modem_status[0]++;
		else if (ra == 11)
			modem_status[1]++;
		else if (ra == 12)
			modem_status[2]++;
		else
			modem_status[3]++;

		compare_graph("modem", a, b, tag * 1000 + blk);
		last = ra;
		n_modem++;
	}

	ref_voice_delete(b);
	voice_delete(a);
	diff_eq_int("the arena is empty", harness_alloc.live, 0, tag);
	diff_eq_int("no bad frees", harness_alloc.bad_free, 0, tag);
	return last;
}

static int
t_modem(void)
{
	diff_begin("voice_modem, both output arms over 48-block runs");

	/*
	 * IN-STREAM (any mode but duplex).  A 2100 Hz tone drives the third
	 * counter past its threshold of 12, so the detector starts appending
	 * two bytes a block from block 13 and the handler's `tx_lin` moves.
	 */
	modem_trial(VOICE_MODE_ONLINE, DETECTOR_ENABLE_ALL, 2, 48, -1, 0, 0, 0, 1);
	/* And with the DLE reset firing part way through. */
	modem_trial(VOICE_MODE_ONLINE, DETECTOR_ENABLE_ALL, 2, 48, 30, 1, 0, 0, 2);
	/* A mode that is neither online nor duplex takes the same arm. */
	modem_trial(VOICE_MODE_RX, DETECTOR_ENABLE_ALL, 2, 48, -1, 0, 0, 0, 3);
	/* Broadband: every tone counter stays at zero, nothing is emitted. */
	modem_trial(VOICE_MODE_ONLINE, DETECTOR_ENABLE_ALL, -1, 48, -1, 0, 0, 0,
		    4);

	/*
	 * STATUS (duplex).  One enable mask per tone, because in status mode
	 * only the LAST arm to fire survives: with everything enabled the
	 * 2225 Hz report overwrites the other three and codes 3, 4 and 5
	 * could never be observed.
	 */
	/*
	 * AND THIS IS THE DEVIATION, ASSERTED RATHER THAN ARGUED (D1011).
	 * `detector_progress` answers 3, 4, 5 and 6 for the four tones, and
	 * `voice_modem`'s chain tests 1, 2 and 4 only -- so 1100 Hz becomes
	 * 12 and the other three are DISCARDED, the handler's own return
	 * going back to the caller instead.  Each trial's last block is the
	 * one that says so: by block 47 every counter is well past its
	 * threshold and the tone reports on every block (F8804).
	 */
	diff_eq_int("1300 Hz (status 3) is discarded",
		    modem_trial(VOICE_MODE_DUPLEX, DETECTOR_ENABLE_1300, 0, 48,
				-1, 0, 0, 0, 5),
		    42, 0);
	diff_eq_int("1100 Hz (status 4) maps to 12",
		    modem_trial(VOICE_MODE_DUPLEX, DETECTOR_ENABLE_1100, 1, 48,
				-1, 0, 0, 0, 6),
		    12, 0);
	diff_eq_int("2100 Hz (status 5) is discarded",
		    modem_trial(VOICE_MODE_DUPLEX, DETECTOR_ENABLE_2100, 2, 48,
				-1, 0, 0, 0, 7),
		    42, 0);
	diff_eq_int("2225 Hz (status 6) is discarded",
		    modem_trial(VOICE_MODE_DUPLEX, DETECTOR_ENABLE_2225, 3, 48,
				-1, 0, 0, 0, 8),
		    42, 0);

	/*
	 * The two cadence codes, forced, one at a time: busy answers 1 and
	 * dial answers 2, which are `voice_modem`'s 10 and 11 arms.  Driven
	 * separately because the dial test runs second inside
	 * `detector_progress` and would otherwise always win.
	 */
	diff_eq_int("a busy cadence (status 1) maps to 10",
		    modem_trial(VOICE_MODE_DUPLEX, DETECTOR_ENABLE_CADENCE, -1,
				48, -1, 0, 1, 0, 9),
		    10, 0);
	diff_eq_int("a dial cadence (status 2) maps to 11",
		    modem_trial(VOICE_MODE_DUPLEX, DETECTOR_ENABLE_CADENCE, -1,
				48, -1, 0, 0, 1, 10),
		    11, 0);

	/*
	 * The DLE reset on the status arm too, and with the OTHER flag: trial
	 * 2 raises <DLE><ETX> alone and this one <DLE><CAN> alone, so neither
	 * half of the object's `||` can be dropped unnoticed.
	 */
	modem_trial(VOICE_MODE_DUPLEX, DETECTOR_ENABLE_ALL, 3, 48, 20, 2, 0, 0,
		    11);
	/* And once with both raised, which is what a real <DLE><CAN> does. */
	modem_trial(VOICE_MODE_ONLINE, DETECTOR_ENABLE_ALL, 2, 48, 24, 3, 0, 0,
		    12);

	return diff_end();
}

/* ------------------------------------------------------------------ */

static int
t_coverage(void)
{
	int i;

	diff_begin("every arm was entered by the RUN, not by the trial list");

	diff_eq_int("voice_create trials", n_create, 2, 0);
	diff_eq_int("debug level 0", seen_level[0], 1, 0);
	diff_eq_int("debug level 2", seen_level[1], 1, 0);
	diff_eq_int("NULL cfg", seen_null_cfg, 1, 0);
	diff_eq_int("the delete line was checked against its own pointer",
		    seen_delete_line, 1, 0);

	diff_eq_int("voice_delete shapes", n_delete_shapes, 32, 0);
	for (i = 0; i < 5; i++) {
		diff_eq_int("delete guard, NULL arm", delete_arm[i][0], 16, i);
		diff_eq_int("delete guard, non-NULL arm", delete_arm[i][1], 16,
			    i);
	}

	diff_eq_int("voice_command trials", n_command > 0, 1, 0);
	for (i = 0; i <= VOICE_COMMAND_MAX; i++)
		diff_eq_int("opcode entered", cmd_arm[i] >= 4, 1, i);
	diff_eq_int("gate refused", cmd_gate[0] >= 4, 1, 0);
	diff_eq_int("gate passed", cmd_gate[1] >= 4, 1, 0);
	diff_eq_int("returned 0", cmd_ret[0] > 0, 1, 0);
	diff_eq_int("returned 7", cmd_ret[1] > 0, 1, 0);

	diff_eq_int("voice_modem blocks", n_modem, 12 * 48, 0);
	diff_eq_int("in-stream arm", modem_mode[0], 5, 0);
	diff_eq_int("status arm", modem_mode[1], 7, 0);
	diff_eq_int("the DLE reset fired", modem_dle[1], 3, 0);
	diff_eq_int("the detector emitted bytes", modem_detlen[1] > 0, 1, 0);
	diff_eq_int("and blocks where it did not", modem_detlen[0] > 0, 1, 0);
	diff_eq_int("the StrmVCE line was filtered, not merely absent",
		    filtered_lines > 0, 1, 0);
	diff_eq_int("voice_modem returned the handler's value",
		    modem_status[3] > 0, 1, 0);
	diff_eq_int("voice_modem mapped status 1 to 10", modem_status[0] > 0,
		    1, 0);
	diff_eq_int("voice_modem mapped status 2 to 11", modem_status[1] > 0,
		    1, 0);
	diff_eq_int("voice_modem mapped status 4 to 12", modem_status[2] > 0,
		    1, 0);

	fprintf(stderr,
		"t_voicesvc: create=%ld delete=%ld command=%ld modem=%ld "
		"blocks; status 10/11/12/pass = %ld/%ld/%ld/%ld; "
		"detlen>0 %ld of %ld\n",
		n_create, n_delete_shapes, n_command, n_modem,
		modem_status[0], modem_status[1], modem_status[2],
		modem_status[3], modem_detlen[1],
		modem_detlen[0] + modem_detlen[1]);
	return diff_end();
}

int
main(void)
{
	int failed = 0;

	failed |= t_create();
	failed |= t_delete_shapes();
	failed |= t_command();
	failed |= t_modem();
	failed |= t_coverage();
	return failed;
}

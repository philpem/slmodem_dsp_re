/*
 * t_faxprocess.c -- differential test of `FAX_process` (`src/service/
 * voice.c`, `.text` 0x001a10, 1,809 bytes).  See `fax.h` for `struct
 * fax_ctx` and `FAX_process`'s own banner in `voice.c` for the full
 * control-flow account this test is checked against.
 *
 * `FAX_create`/`fax_class1_create` are NOT written (blocked on `faxvmi.c`,
 * F10059), so this test builds `struct fax_ctx` and `struct fax_class1`
 * fixtures by hand, the same technique `t_faxdelete.c` uses for `fax_ctx`
 * and `t_class1progress.c` uses for `fax_class1` -- both start every field
 * zeroed, which is a fixture choice (there is no written constructor to
 * compare against), not a claim about what `FAX_create` itself would leave.
 *
 * DRIVING ALL ELEVEN DISPATCH CASES (PLUS THE INVALID DEFAULT) WITHOUT A
 * REAL T.30 SESSION.  `fax_class1_progress`'s own documented step 6 applies
 * `status = delayed_status` whenever `delayed_status_countdown` reaches
 * zero on a call (`class1.h`), and `_idle_state` -- CLASS1_IDLE_STATE's own
 * handler, already tested by `t_class1progress.c`, and documented as
 * reading no field of the session -- otherwise leaves `status` at
 * `FAX_CLASS1_NO_MESSAGE`.  So setting `delayed_status_countdown = 1` and
 * `delayed_status` to the status under test, with `count` set to exactly
 * one `host_frame_samples` chunk, makes `fax_class1_progress`'s return for
 * THAT ONE FLUSH exactly the value under test -- including 11, past every
 * real `FAX_CLASS1_*` code, to reach FAX_process's own "Unknown status"
 * default arm.  This is the object's own documented mechanism, not a
 * bypass of it.
 *
 * WHAT IS NOT DRIVEN: a REAL rate ratio that resamples an exact
 * `host_frame_samples`-sample block to exactly 0xa0 output samples.  The
 * "real resampler" trials below use `host_frame_samples = 0xa0` with a
 * genuine non-identity `rc_a`/`rc_b` pair anyway; `RcFixed_Resample`'s own
 * ratio then almost never returns exactly 0xa0 from an 0xa0-sample input,
 * so these trials are EXPECTED to land on FAX_process's own sample-count-
 * mismatch branch on both sides -- which is itself a real, faithfully
 * compared path (the mismatch debug line plus the -1 accumulator
 * sentinel), not a gap.  A configuration that instead reaches the
 * post-resample `fax_class1_progress` call with a real resampler installed
 * is left for a follow-up that wants to spend the effort finding an exact
 * ratio/chunk-size pair; nothing about the mismatch path being exercised
 * instead is a shortcut around it.
 */

#include <stddef.h>
#include <string.h>

#include "harness.h"
#include "dsplib/class1.h"
#include "dsplib/debug.h"
#include "dsplib/fax.h"
#include "dsplib/fixedrc.h"
#include "dsplib/sysdep.h"

extern unsigned int ref_dsplibs_debug_level;

extern int ref_FAX_process(struct fax_ctx *ctx, const void *in, void *out,
			   int count);
extern int ref__idle_state(struct fax_class1 *ctx, const short *rx,
	short *tx, int word3, int word4, int *rx_count, int *tx_count,
	int word7, int *word8);
extern class1_state_fn ref_class1_state_functions[19];

/*
 * FILE-LOCAL in the object, so class1.c defines it `static` and class1.h no
 * longer declares it.  Its address is taken (class1.c installs it into
 * `class1_state_functions`, and this test does too), so the ordinary calling
 * convention is unchanged; the test tier links a globalized copy
 * (tools/testvisible.py).
 */
extern int _idle_state(struct fax_class1 *ctx, const short *rx, short *tx,
		       int word3, int word4, int *rx_count, int *tx_count,
		       int word7, int *word8);

extern struct rc *ref_RcFixed_Create(int mode);
extern void ref_RcFixed_Delete(struct rc *h);

/* ------------------------------------------------------------------- */

#define BUFBYTES	8192

static unsigned char in_ours[BUFBYTES], in_ref[BUFBYTES];
static unsigned char out_ours[BUFBYTES], out_ref[BUFBYTES];

static void
fill_buffers(long seed)
{
	int i;
	unsigned v = (unsigned)seed * 2654435761u + 12345u;

	for (i = 0; i < BUFBYTES; i++) {
		v = v * 1103515245u + 12345u;
		in_ours[i] = in_ref[i] = (unsigned char)(v >> 16);
		v = v * 1103515245u + 12345u;
		out_ours[i] = out_ref[i] = (unsigned char)(0xa5 ^ (v >> 20));
	}
}

/* ------------------------------------------------------------------- */
/* `struct fax_ctx`/`struct fax_class1` fixtures, one heap copy per side. */

static void
init_class1(struct fax_class1 *c, int delayed_countdown, int delayed_status)
{
	memset(c, 0, sizeof(*c));
	c->state = CLASS1_IDLE_STATE;
	c->prev_state = CLASS1_IDLE_STATE;
	c->delayed_status_countdown = delayed_countdown;
	c->delayed_status = delayed_status;
}

struct fax_case {
	int host_frame_samples;
	int host_rx_enable;
	int host_rx_want;
	int use_rc;		/* 0 = identity (rc_a/rc_b NULL), 1 = real  */
	int delayed_countdown;
	int delayed_status;
	int count;
	unsigned level;
};

static void
normalise_ctx(struct fax_ctx *c)
{
	c->modem = NULL;
	c->class1 = NULL;
	c->rc_a = NULL;
	c->rc_b = NULL;
}

static long tag_counter;
static long objects_compared;

static void
run_case(const struct fax_case *tc)
{
	struct fax_ctx *ca, *cb;
	struct fax_class1 c1a, c1b;
	struct fax_ctx norm_a, norm_b;
	struct rc *rc_a_ours = NULL, *rc_b_ours = NULL;
	struct rc *rc_a_ref = NULL, *rc_b_ref = NULL;
	int mode = -1;
	int ra, rb;
	long tag = tag_counter++;
	int modem_a, modem_b;
	int i, differ;

	harness_alloc_reset();
	harness_tty_reset();
	fill_buffers(tag);

	if (tc->use_rc) {
		mode = RcFixed_Check_Combination(9600, 8000);
		rc_a_ours = RcFixed_Create(mode);
		rc_b_ours = RcFixed_Create(mode);
		rc_a_ref = ref_RcFixed_Create(mode);
		rc_b_ref = ref_RcFixed_Create(mode);
	}

	init_class1(&c1a, tc->delayed_countdown, tc->delayed_status);
	init_class1(&c1b, tc->delayed_countdown, tc->delayed_status);

	ca = sysdep_malloc(sizeof(*ca));
	cb = sysdep_malloc(sizeof(*cb));
	memset(ca, 0, sizeof(*ca));
	memset(cb, 0, sizeof(*cb));

	ca->modem = &modem_a;
	cb->modem = &modem_b;
	ca->class1 = &c1a;
	cb->class1 = &c1b;
	ca->rc_a = rc_a_ours;
	cb->rc_a = rc_a_ref;
	ca->rc_b = rc_b_ours;
	cb->rc_b = rc_b_ref;
	ca->host_frame_samples = tc->host_frame_samples;
	cb->host_frame_samples = tc->host_frame_samples;
	ca->host_rx_enable = tc->host_rx_enable;
	cb->host_rx_enable = tc->host_rx_enable;
	ca->host_rx_want = tc->host_rx_want;
	cb->host_rx_want = tc->host_rx_want;

	{
		static unsigned char ttyin[BUFBYTES];
		unsigned v = (unsigned)tag * 747796405u + 2891336453u;

		for (i = 0; i < BUFBYTES; i++) {
			v = v * 747796405u + 2891336453u;
			ttyin[i] = (unsigned char)(v >> 15);
		}
		harness_ttyin_reset(ttyin, BUFBYTES);
	}

	dsplibs_debug_level = ref_dsplibs_debug_level = tc->level;
	dsplib_debug_capture_reset();
	dsplib_debug_capture_on = 1;

	ra = FAX_process(ca, in_ours, out_ours, tc->count);
	rb = ref_FAX_process(cb, in_ref, out_ref, tc->count);

	dsplib_debug_capture_on = 0;
	dsplibs_debug_level = ref_dsplibs_debug_level = 0;

	diff_eq_int("FAX_process return, input %ld", ra, rb, tag);

	norm_a = *ca;
	norm_b = *cb;
	normalise_ctx(&norm_a);
	normalise_ctx(&norm_b);
	diff_eq_obj("fax_ctx after process", struct fax_ctx, &norm_a, &norm_b,
		   tag);
	objects_compared++;

	diff_eq_obj("fax_class1 after process", struct fax_class1, &c1a,
		   &c1b, tag);

	differ = memcmp(in_ours, in_ref, BUFBYTES) != 0 ? 1 : 0;
	diff_eq_int("input buffer, ours vs ref, input %ld", differ, 0, tag);
	differ = memcmp(out_ours, out_ref, BUFBYTES) != 0 ? 1 : 0;
	diff_eq_int("output buffer, ours vs ref, input %ld", differ, 0, tag);

	diff_eq_int("tty out len, input %ld", (long)harness_tty_ours.len,
		    (long)harness_tty_ref.len, tag);
	diff_eq_int("tty out calls, input %ld", (long)harness_tty_ours.calls,
		    (long)harness_tty_ref.calls, tag);
	differ = memcmp(harness_tty_ours.data, harness_tty_ref.data,
	    sizeof(harness_tty_ours.data)) != 0 ? 1 : 0;
	diff_eq_int("tty out bytes, input %ld", differ, 0, tag);
	diff_eq_int("tty in calls, input %ld", (long)harness_ttyin_ours.calls,
		    (long)harness_ttyin_ref.calls, tag);
	diff_eq_int("tty in bytes taken, input %ld",
		    (long)harness_ttyin_ours.bytes,
		    (long)harness_ttyin_ref.bytes, tag);

	diff_eq_int("debug transcript, input %ld",
		    strcmp(dsplib_debug_capture_text(0),
			   dsplib_debug_capture_text(1)) == 0, 1, tag);

	if (rc_a_ours != NULL)
		RcFixed_Delete(rc_a_ours);
	if (rc_b_ours != NULL)
		RcFixed_Delete(rc_b_ours);
	if (rc_a_ref != NULL)
		ref_RcFixed_Delete(rc_a_ref);
	if (rc_b_ref != NULL)
		ref_RcFixed_Delete(rc_b_ref);
	sysdep_free(ca);
	sysdep_free(cb);
}

/* ------------------------------------------------------------------- */

static int
test_dispatch(void)
{
	/* every FAX_CLASS1_* code, 0..10, plus one past the end (11). */
	static const int statuses[] = {
		FAX_CLASS1_NO_MESSAGE, FAX_CLASS1_OK, FAX_CLASS1_ERROR,
		FAX_CLASS1_OK_NO_CARRIER, FAX_CLASS1_ERROR_NO_CARRIER,
		FAX_CLASS1_ERROR_ON_HOOK, FAX_CLASS1_CONNECT,
		FAX_CLASS1_NO_CARRIER, FAX_CLASS1_NO_CARRIER_NO_MESSAGE,
		FAX_CLASS1_OTHER_CARRIER, FAX_CLASS1_ACCEPT_RATE, 11
	};
	int i, level, en;

	diff_begin("FAX_process: dispatch table, every status");

	for (i = 0; i < (int)(sizeof(statuses) / sizeof(statuses[0])); i++) {
		for (level = 0; level <= 2; level++) {
			for (en = 0; en <= 1; en++) {
				struct fax_case tc;

				tc.host_frame_samples = 0xa0;
				tc.host_rx_enable = en;
				tc.host_rx_want = en ? 37 : 0;
				tc.use_rc = 0;
				tc.delayed_countdown = 1;
				tc.delayed_status = statuses[i];
				tc.count = 0xa0;
				tc.level = (unsigned)level;
				run_case(&tc);
			}
		}
	}

	return diff_end();
}

static int
test_loop_mechanics(void)
{
	static const int counts[] = { 1, 40, 0xa0, 250, 480, 500, 2000 };
	static const int wants[] = { 0, 1, 100, 0x1000, 5000 };
	int i, j, level;

	diff_begin("FAX_process: outer/inner loop mechanics, identity path");

	for (i = 0; i < (int)(sizeof(counts) / sizeof(counts[0])); i++) {
		for (j = 0; j < (int)(sizeof(wants) / sizeof(wants[0])); j++) {
			for (level = 0; level <= 2; level += 2) {
				struct fax_case tc;

				tc.host_frame_samples = 0xa0;
				tc.host_rx_enable = 1;
				tc.host_rx_want = wants[j];
				tc.use_rc = 0;
				tc.delayed_countdown = 0;
				tc.delayed_status = 0;
				tc.count = counts[i];
				tc.level = (unsigned)level;
				run_case(&tc);
			}
		}
	}

	return diff_end();
}

static int
test_real_resampler(void)
{
	static const int counts[] = { 0xa0, 480, 500 };
	int i, level;

	diff_begin("FAX_process: real rc_a/rc_b resampler installed");

	for (i = 0; i < (int)(sizeof(counts) / sizeof(counts[0])); i++) {
		for (level = 1; level <= 2; level++) {
			struct fax_case tc;

			tc.host_frame_samples = 0xa0;
			tc.host_rx_enable = 1;
			tc.host_rx_want = 64;
			tc.use_rc = 1;
			tc.delayed_countdown = 0;
			tc.delayed_status = 0;
			tc.count = counts[i];
			tc.level = (unsigned)level;
			run_case(&tc);
		}
	}

	return diff_end();
}

static int
test_frame_mismatch(void)
{
	int level;

	diff_begin("FAX_process: identity path, host_frame_samples != 0xa0");

	for (level = 1; level <= 2; level++) {
		struct fax_case tc;

		tc.host_frame_samples = 0x50;
		tc.host_rx_enable = 1;
		tc.host_rx_want = 20;
		tc.use_rc = 0;
		tc.delayed_countdown = 0;
		tc.delayed_status = 0;
		tc.count = 0x50 * 3;
		tc.level = (unsigned)level;
		run_case(&tc);
	}

	return diff_end();
}

int
main(void)
{
	int rc = 0;

	class1_state_functions[CLASS1_IDLE_STATE] = _idle_state;
	ref_class1_state_functions[CLASS1_IDLE_STATE] = ref__idle_state;

	rc |= test_dispatch();
	rc |= test_loop_mechanics();
	rc |= test_real_resampler();
	rc |= test_frame_mismatch();

	(void)objects_compared;

	return rc;
}

/*
 * t_class1cmd.c -- differential test of `fax_class1_command` and
 * `_answer_tone_state` (class1.c).
 *
 * `fax_class1_command`'s TH/RH commands drive `ctx->vmi_c`/`ctx->vmi_a`
 * through the already-tested `cHDLCtx_preamble_state_init`/
 * `_cHDLCrx_init_from_idle` (F10119); TM/RM drive the data-mode modem
 * through the already-tested `_tx_scrambled_ones_init`/
 * `_rx_look_carrier_init`.  A session built by `fax_class1_create` (also
 * this wave) supplies real `vmi_c`/`vmi_a` the same way any real caller
 * would, rather than a synthetic fixture.
 */

#include <stddef.h>
#include <string.h>

#include "harness.h"
#include "dsplib/class1.h"
#include "dsplib/class1tx.h"
#include "dsplib/debug.h"
#include "dsplib/fpm_tone.h"

extern struct fax_class1 *ref_fax_class1_create(struct fax_class1 *existing,
						const struct fax_class1_cfg *cfg);
extern int ref_fax_class1_command(struct fax_class1 *ctx, int cmd, int arg3,
				  int arg4);
extern int ref__answer_tone_state(void *ctx, const short *rx, short *tx,
				  int word3, int word4, int *rx_count,
				  int *tx_count, int word7, int *word8);

/*
 * FILE-LOCAL in the object, so class1.c defines it `static` and class1.h no
 * longer declares it.  Its address is taken (class1.c installs it into
 * `class1_state_functions`), so the ordinary calling convention is
 * unchanged; the test tier links a globalized copy (tools/testvisible.py).
 */
extern int _answer_tone_state(struct fax_class1 *ctx, const short *rx,
			      short *tx, int word3, int word4,
			      int *rx_count, int *tx_count, int word7,
			      int *word8);

extern unsigned int ref_dsplibs_debug_level;

static struct fax_class1 *
make_pair(struct fax_class1 **out_a)
{
	struct fax_class1_cfg cfg;
	struct fax_class1 *a, *b;

	memset(&cfg, 0, sizeof(cfg));
	cfg.mode = CLASS1_ANS_ORG_NORMAL;
	cfg.s7_timeout = 60;
	cfg.iir_enable = 1;

	a = ref_fax_class1_create(NULL, &cfg);
	b = fax_class1_create(NULL, &cfg);
	*out_a = a;
	return b;
}

static void
cmp_ctx(const char *what, struct fax_class1 *a, struct fax_class1 *b,
       long tag)
{
	char buf[64];

#define FLD(name) \
	do { \
		(void)snprintf(buf, sizeof(buf), "%s.%s (%%ld)", what, #name); \
		diff_eq_int(buf, b->name, a->name, tag); \
	} while (0)
	FLD(state);
	FLD(modem_rate_code);
	FLD(superframe_len);
	FLD(superframe_countdown);
	FLD(modem_direction);
	FLD(silence_blocks);
	FLD(countdown);
	FLD(energy);
#undef FLD
}

static int
run_th(long tag)
{
	struct fax_class1 *a, *b;
	int ra, rb;

	b = make_pair(&a);

	diff_begin("fax_class1_command: FAX_CLASS1_TH_COMMAND");
	ra = ref_fax_class1_command(a, FAX_CLASS1_TH_COMMAND, 3, 0);
	rb = fax_class1_command(b, FAX_CLASS1_TH_COMMAND, 3, 0);
	diff_eq_int("ret (%ld)", rb, ra, tag);
	cmp_ctx("TH", a, b, tag);
	return diff_end();
}

static int
run_rh(long tag)
{
	struct fax_class1 *a, *b;
	int ra, rb;

	b = make_pair(&a);

	diff_begin("fax_class1_command: FAX_CLASS1_RH_COMMAND, from idle");
	ra = ref_fax_class1_command(a, FAX_CLASS1_RH_COMMAND, 3, 0);
	rb = fax_class1_command(b, FAX_CLASS1_RH_COMMAND, 3, 0);
	diff_eq_int("ret (%ld)", rb, ra, tag);
	cmp_ctx("RH idle", a, b, tag);
	return diff_end();
}

/* RH while already HDLC_RECEIVE_BETWEEN_BUFFERS_STATE, same rate: restart. */
static int
run_rh_restart(long tag)
{
	struct fax_class1 *a, *b;
	int ra, rb;

	b = make_pair(&a);
	a->state = b->state = CLASS1_HDLC_RECEIVE_BETWEEN_BUFFERS_STATE;
	a->modem_rate_code = b->modem_rate_code = 3;

	diff_begin("fax_class1_command: RH, restart");
	ra = ref_fax_class1_command(a, FAX_CLASS1_RH_COMMAND, 3, 0);
	rb = fax_class1_command(b, FAX_CLASS1_RH_COMMAND, 3, 0);
	diff_eq_int("ret (%ld)", rb, ra, tag);
	cmp_ctx("RH restart", a, b, tag);
	return diff_end();
}

/* RH while between-buffers with buffered data (superframe_len != 0): emulate. */
static int
run_rh_emulate(long tag)
{
	struct fax_class1 *a, *b;
	int ra, rb;

	b = make_pair(&a);
	a->state = b->state = CLASS1_HDLC_RECEIVE_BETWEEN_BUFFERS_STATE;
	a->modem_rate_code = b->modem_rate_code = 5;	/* != arg3, forces else */
	a->superframe_len = b->superframe_len = 42;

	diff_begin("fax_class1_command: RH, emulate");
	ra = ref_fax_class1_command(a, FAX_CLASS1_RH_COMMAND, 3, 0);
	rb = fax_class1_command(b, FAX_CLASS1_RH_COMMAND, 3, 0);
	diff_eq_int("ret (%ld)", rb, ra, tag);
	cmp_ctx("RH emulate", a, b, tag);
	return diff_end();
}

static int
run_tm(int rate_code, long tag)
{
	struct fax_class1 *a, *b;
	int ra, rb;

	b = make_pair(&a);

	diff_begin("fax_class1_command: FAX_CLASS1_TM_COMMAND");
	ra = ref_fax_class1_command(a, FAX_CLASS1_TM_COMMAND, rate_code, 80);
	rb = fax_class1_command(b, FAX_CLASS1_TM_COMMAND, rate_code, 80);
	diff_eq_int("ret (%ld)", rb, ra, tag);
	cmp_ctx("TM", a, b, tag);
	return diff_end();
}

static int
run_rm(int rate_code, long tag)
{
	struct fax_class1 *a, *b;
	int ra, rb;

	b = make_pair(&a);

	diff_begin("fax_class1_command: FAX_CLASS1_RM_COMMAND");
	ra = ref_fax_class1_command(a, FAX_CLASS1_RM_COMMAND, rate_code, 0);
	rb = fax_class1_command(b, FAX_CLASS1_RM_COMMAND, rate_code, 0);
	diff_eq_int("ret (%ld)", rb, ra, tag);
	cmp_ctx("RM", a, b, tag);
	return diff_end();
}

/* TM then RM: tears down the just-built TX modem (modem_direction == 2 check). */
static int
run_tm_then_rm(long tag)
{
	struct fax_class1 *a, *b;
	int ra, rb;

	b = make_pair(&a);

	(void)ref_fax_class1_command(a, FAX_CLASS1_TM_COMMAND, 0x18, 80);
	(void)fax_class1_command(b, FAX_CLASS1_TM_COMMAND, 0x18, 80);

	diff_begin("fax_class1_command: TM then RM");
	ra = ref_fax_class1_command(a, FAX_CLASS1_RM_COMMAND, 0x18, 0);
	rb = fax_class1_command(b, FAX_CLASS1_RM_COMMAND, 0x18, 0);
	diff_eq_int("ret (%ld)", rb, ra, tag);
	cmp_ctx("TM then RM", a, b, tag);
	return diff_end();
}

/* RM then TM: tears down the just-built RX modem (modem_direction == 1 check). */
static int
run_rm_then_tm(long tag)
{
	struct fax_class1 *a, *b;
	int ra, rb;

	b = make_pair(&a);

	(void)ref_fax_class1_command(a, FAX_CLASS1_RM_COMMAND, 0x60, 0);
	(void)fax_class1_command(b, FAX_CLASS1_RM_COMMAND, 0x60, 0);

	diff_begin("fax_class1_command: RM then TM");
	ra = ref_fax_class1_command(a, FAX_CLASS1_TM_COMMAND, 0x60, 40);
	rb = fax_class1_command(b, FAX_CLASS1_TM_COMMAND, 0x60, 40);
	diff_eq_int("ret (%ld)", rb, ra, tag);
	cmp_ctx("RM then TM", a, b, tag);
	return diff_end();
}

static int
run_ts(int ms, long tag)
{
	struct fax_class1 *a, *b;
	int ra, rb;

	b = make_pair(&a);

	diff_begin("fax_class1_command: FAX_CLASS1_TS_COMMAND");
	ra = ref_fax_class1_command(a, FAX_CLASS1_TS_COMMAND, ms, 0);
	rb = fax_class1_command(b, FAX_CLASS1_TS_COMMAND, ms, 0);
	diff_eq_int("ret (%ld)", rb, ra, tag);
	cmp_ctx("TS", a, b, tag);
	return diff_end();
}

static int
run_rs(int ms, long tag)
{
	struct fax_class1 *a, *b;
	int ra, rb;

	b = make_pair(&a);
	a->energy = b->energy = 12345;
	a->silence_blocks = b->silence_blocks = 77;

	diff_begin("fax_class1_command: FAX_CLASS1_RS_COMMAND");
	ra = ref_fax_class1_command(a, FAX_CLASS1_RS_COMMAND, ms, 0);
	rb = fax_class1_command(b, FAX_CLASS1_RS_COMMAND, ms, 0);
	diff_eq_int("ret (%ld)", rb, ra, tag);
	cmp_ctx("RS", a, b, tag);
	return diff_end();
}

/* cmd out of range: no dispatch, still returns 1, superframe_len still cleared. */
static int
run_bad_cmd(int cmd, long tag)
{
	struct fax_class1 *a, *b;
	int ra, rb;

	b = make_pair(&a);
	a->superframe_len = b->superframe_len = 77;

	diff_begin("fax_class1_command: cmd out of range");
	ra = ref_fax_class1_command(a, cmd, 0, 0);
	rb = fax_class1_command(b, cmd, 0, 0);
	diff_eq_int("ret (%ld)", rb, ra, tag);
	cmp_ctx("bad cmd", a, b, tag);
	return diff_end();
}

static int
run_debug_on(void)
{
	struct fax_class1 *a, *b;
	int rc = 0;

	dsplibs_debug_level = ref_dsplibs_debug_level = 2;

	b = make_pair(&a);
	diff_begin("fax_class1_command: debug_level > 1, TH");
	(void)ref_fax_class1_command(a, FAX_CLASS1_TH_COMMAND, 3, 0);
	(void)fax_class1_command(b, FAX_CLASS1_TH_COMMAND, 3, 0);
	cmp_ctx("debug TH", a, b, 900);
	rc |= diff_end();

	b = make_pair(&a);
	diff_begin("fax_class1_command: debug_level > 1, bad cmd (uninit name)");
	(void)ref_fax_class1_command(a, -1, 0, 0);
	(void)fax_class1_command(b, -1, 0, 0);
	cmp_ctx("debug bad cmd", a, b, 901);
	rc |= diff_end();

	dsplibs_debug_level = ref_dsplibs_debug_level = 0;
	return rc;
}

/* _answer_tone_state: standalone, real FPM_TONE_create-built generator. */
static void
cmp_tx(const char *what, short *a, short *b, int n, long tag)
{
	int i;
	char buf[80];

	for (i = 0; i < n; i++) {
		(void)snprintf(buf, sizeof(buf), "%s[%d] (%%ld)", what, i);
		diff_eq_int(buf, b[i], a[i], tag);
	}
}

static int
run_answer_tone(long tag)
{
	struct fax_class1_cfg cfg;
	struct fax_class1 *a, *b;
	short rx[160], tx_a[160], tx_b[160];
	int rx_count, tx_count_a, tx_count_b, word8;
	int ra, rb, i, rc = 0;

	memset(&cfg, 0, sizeof(cfg));
	cfg.mode = CLASS1_ANS_ORG_ANSWER;
	cfg.answer_tone_ms = 40;	/* answer_tone_blocks == 2 */

	a = ref_fax_class1_create(NULL, &cfg);
	b = fax_class1_create(NULL, &cfg);

	memset(rx, 0, sizeof(rx));

	/*
	 * Three calls: the first two stay under the threshold (real tone
	 * generated each time), the third crosses it and hands off to
	 * `cHDLCtx_preamble_state_init` -- exercising both branches against
	 * the real `vmi_c` `fax_class1_create` built.
	 */
	for (i = 0; i < 3; i++) {
		memset(tx_a, 0xaa, sizeof(tx_a));
		memset(tx_b, 0xaa, sizeof(tx_b));

		diff_begin("_answer_tone_state");
		rx_count = 160;
		tx_count_a = tx_count_b = 0;
		word8 = 0;
		ra = ref__answer_tone_state(a, rx, tx_a, 0, 0, &rx_count,
					    &tx_count_a, 0, &word8);
		rb = _answer_tone_state(b, rx, tx_b, 0, 0, &rx_count,
					&tx_count_b, 0, &word8);
		diff_eq_int("ret (%ld)", rb, ra, tag + i);
		diff_eq_int("tx_count (%ld)", tx_count_b, tx_count_a, tag + i);
		diff_eq_int("countdown (%ld)", b->countdown, a->countdown,
			    tag + i);
		diff_eq_int("state (%ld)", b->state, a->state, tag + i);
		cmp_tx("tx samples", tx_a, tx_b, 160, tag + i);
		rc |= diff_end();
	}

	return rc;
}

int
main(void)
{
	int rc = 0;

	rc |= run_th(10);
	rc |= run_rh(11);
	rc |= run_rh_restart(12);
	rc |= run_rh_emulate(13);

	rc |= run_tm(0x18, 20);
	rc |= run_tm(0x91, 21);
	rc |= run_rm(0x18, 22);
	rc |= run_rm(0x91, 23);
	rc |= run_tm_then_rm(24);
	rc |= run_rm_then_tm(25);

	rc |= run_ts(3000, 30);
	rc |= run_ts(0, 31);
	rc |= run_rs(3000, 32);

	rc |= run_bad_cmd(6, 40);
	rc |= run_bad_cmd(-1, 41);
	rc |= run_bad_cmd(99, 42);

	rc |= run_debug_on();

	rc |= run_answer_tone(50);

	return rc;
}

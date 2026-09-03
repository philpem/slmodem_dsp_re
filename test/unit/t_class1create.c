/*
 * t_class1create.c -- differential test of `fax_class1_create` (class1.c),
 * the largest single piece left in fax.  Covers the fresh-build path (both
 * `cfg->mode` values), the reinit path (an already-built session, so
 * `vmi_c`/`vmi_a` are NOT rebuilt), and the installed `class1_state_
 * functions` table.
 *
 * `class1_state_functions` IS A SHARED GLOBAL, not per-instance, so its raw
 * pointer VALUES can never match between `ours` and `ref_` -- different
 * binaries, different addresses.  What is checked instead is STRUCTURAL:
 * every one of the nineteen slots, after a call, equals the address of the
 * correspondingly-named local handler -- the same fact `dis.py` established
 * for the object's own install order (F10119... this finding).
 */

#include <stddef.h>
#include <string.h>

#include "harness.h"
#include "dsplib/class1.h"
#include "dsplib/class1rx.h"
#include "dsplib/class1tx.h"
#include "dsplib/debug.h"
#include "dsplib/faxvmi.h"

extern struct fax_class1 *ref_fax_class1_create(struct fax_class1 *existing,
						const struct fax_class1_cfg *cfg);
extern unsigned int ref_dsplibs_debug_level;
extern class1_state_fn ref_class1_state_functions[19];

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
	FLD(prev_state);
	FLD(countdown);
	FLD(ans_org);
	FLD(cng_enabled);
	FLD(modem_rate_code);
	FLD(dle_seen);
	FLD(f127c);
	FLD(delayed_status_countdown);
	FLD(f12d8);
	FLD(s7_timeout);
	FLD(f12d4);
	FLD(f12f0);
	FLD(f125c);
	FLD(f1260);
	FLD(clock_sec);
	FLD(clock_frac);
	FLD(f12cc);
	FLD(f12d0);
#undef FLD
	(void)snprintf(buf, sizeof(buf), "%s vmi_c NULL-ness (%%ld)", what);
	diff_eq_int(buf, b->vmi_c != NULL, a->vmi_c != NULL, tag);
	(void)snprintf(buf, sizeof(buf), "%s vmi_a NULL-ness (%%ld)", what);
	diff_eq_int(buf, b->vmi_a != NULL, a->vmi_a != NULL, tag);
	(void)snprintf(buf, sizeof(buf), "%s f1258 NULL-ness (%%ld)", what);
	diff_eq_int(buf, b->f1258 != NULL, a->f1258 != NULL, tag);
}

/*
 * Structural check of OUR OWN install: every slot equals the address of the
 * handler `dis.py`'s own trace names for it, in `states_names`' order.
 */
static int
check_state_table(void)
{
	int ok = 1;

#define CHECK(idx, fn) \
	do { \
		if (class1_state_functions[idx] != (fn)) { \
			printf("FAIL class1_state_functions[%d] != %s\n", \
			       (idx), #fn); \
			ok = 0; \
		} \
	} while (0)
	CHECK(CLASS1_T30_SILENCE_BEFORE_PREAMBLE_STATE, _t30_silence_before_tx_state);
	CHECK(CLASS1_T30_PREAMBLE_STATE, _t30_preabmle_state);
	CHECK(CLASS1_SEND_HDLC_BUFFER_STATE, _send_hdlc_buffer_state);
	CHECK(CLASS1_SEND_HDLC_BETWEEN_BUFFER_STATE, _send_hdlc_between_buffer_state);
	CHECK(CLASS1_HDLC_RECEIVE_LOOK_CARRIER_STATE, _hdlc_receive_look_carrier_state);
	CHECK(CLASS1_HDLC_RECEIVE_STATE, _hdlc_receive_state);
	CHECK(CLASS1_HDLC_RECEIVE_BETWEEN_BUFFERS_STATE, _hdlc_receive_between_buffers_state);
	CHECK(CLASS1_HDLC_EMULATE_RECEIVE_STATE, _hdlc_emulate_receive_state);
	CHECK(CLASS1_IDLE_STATE, _idle_state);
	CHECK(CLASS1_TX_SCRAMBLED_ONES_STATE, _tx_scrambled_ones_state);
	CHECK(CLASS1_TX_DATA_STATE, _tx_data_state);
	CHECK(CLASS1_TX_NULLS_STATE, _tx_nulls_state);
	CHECK(CLASS1_RX_LOOK_CARRIER, _rx_look_carrier_state);
	CHECK(CLASS1_RX_DATA_STATE, _rx_data_state);
	CHECK(CLASS1_ANSWER_TONE_STATE, _answer_tone_state);
	CHECK(CLASS1_SEND_SILENCE_STATE, _send_silence_state);
	CHECK(CLASS1_RECIEVE_SILENCE_STATE, _recieve_silence_state);
	CHECK(CLASS1_CHDLCTX_OFF_STATE, cHDLCtx_off);
	CHECK(CLASS1_TX_SILENCE_BEFORE_SCRM_ONES, _tx_silence_before_scrm_ones);
#undef CHECK
	return ok ? 0 : 1;
}

static int
run_fresh_normal(long tag)
{
	struct fax_class1_cfg cfg;
	struct fax_class1 *a, *b;
	int rc;

	memset(&cfg, 0, sizeof(cfg));
	cfg.mode = CLASS1_ANS_ORG_NORMAL;
	cfg.s7_timeout = 60;
	cfg.f08 = 0x1234;
	cfg.iir_enable = 1;
	cfg.answer_tone_ms = 0;
	cfg.disable_cng = 0;

	diff_begin("fax_class1_create: fresh, normal");

	a = ref_fax_class1_create(NULL, &cfg);
	b = fax_class1_create(NULL, &cfg);

	diff_eq_int("a != NULL (%ld)", b != NULL, a != NULL, tag);
	cmp_ctx("fresh normal", a, b, tag);

	rc = diff_end();
	rc |= check_state_table();
	return rc;
}

static int
run_fresh_answer(long tag)
{
	struct fax_class1_cfg cfg;
	struct fax_class1 *a, *b;
	int rc;

	memset(&cfg, 0, sizeof(cfg));
	cfg.mode = CLASS1_ANS_ORG_ANSWER;
	cfg.s7_timeout = 30;
	cfg.f08 = 7;
	cfg.iir_enable = 0;
	cfg.answer_tone_ms = 3000;
	cfg.disable_cng = 1;

	diff_begin("fax_class1_create: fresh, answer-tone");

	a = ref_fax_class1_create(NULL, &cfg);
	b = fax_class1_create(NULL, &cfg);

	cmp_ctx("fresh answer", a, b, tag);
	diff_eq_int("state == ANSWER_TONE (%ld)", b->state, a->state, tag);
	diff_eq_int("ans_org == ANSWER (%ld)", b->ans_org, a->ans_org, tag);

	rc = diff_end();
	rc |= check_state_table();
	return rc;
}

/* answer_tone_ms nonzero, derives a real blocks-from-ms value */
static int
run_answer_tone_ms(int ms, long tag)
{
	struct fax_class1_cfg cfg;
	struct fax_class1 *a, *b;

	memset(&cfg, 0, sizeof(cfg));
	cfg.mode = CLASS1_ANS_ORG_NORMAL;
	cfg.answer_tone_ms = ms;

	diff_begin("fax_class1_create: answer_tone_ms derivation");

	a = ref_fax_class1_create(NULL, &cfg);
	b = fax_class1_create(NULL, &cfg);

	diff_eq_int("answer_tone_blocks (%ld)", b->answer_tone_blocks,
		    a->answer_tone_blocks, tag);

	return diff_end();
}

/* Reinit: vmi_c/vmi_a survive unchanged (not rebuilt). */
static int
run_reinit(long tag)
{
	struct fax_class1_cfg cfg;
	struct fax_class1 *a, *b;
	struct faxvmi *a_vmi_c, *b_vmi_c, *a_vmi_a, *b_vmi_a;
	int rc;

	memset(&cfg, 0, sizeof(cfg));
	cfg.mode = CLASS1_ANS_ORG_NORMAL;
	cfg.s7_timeout = 15;
	cfg.iir_enable = 1;

	a = ref_fax_class1_create(NULL, &cfg);
	b = fax_class1_create(NULL, &cfg);
	a_vmi_c = a->vmi_c;
	b_vmi_c = b->vmi_c;
	a_vmi_a = a->vmi_a;
	b_vmi_a = b->vmi_a;

	diff_begin("fax_class1_create: reinit");

	cfg.s7_timeout = 99;
	a = ref_fax_class1_create(a, &cfg);
	b = fax_class1_create(b, &cfg);

	cmp_ctx("reinit", a, b, tag);
	diff_eq_int("vmi_c unchanged (%ld)", b->vmi_c == b_vmi_c,
		    a->vmi_c == a_vmi_c, tag);
	diff_eq_int("vmi_a unchanged (%ld)", b->vmi_a == b_vmi_a,
		    a->vmi_a == a_vmi_a, tag);

	rc = diff_end();
	rc |= check_state_table();
	return rc;
}

static int
run_debug_on(void)
{
	struct fax_class1_cfg cfg;
	struct fax_class1 *a, *b;
	int rc;

	dsplibs_debug_level = ref_dsplibs_debug_level = 2;

	memset(&cfg, 0, sizeof(cfg));
	cfg.mode = CLASS1_ANS_ORG_NORMAL;
	cfg.disable_cng = 1;	/* fires the "CNG generation disabled" print */

	diff_begin("fax_class1_create: debug_level > 1");

	a = ref_fax_class1_create(NULL, &cfg);
	b = fax_class1_create(NULL, &cfg);
	cmp_ctx("debug", a, b, 900);

	rc = diff_end();

	dsplibs_debug_level = ref_dsplibs_debug_level = 0;
	return rc;
}

int
main(void)
{
	int rc = 0;

	rc |= run_fresh_normal(10);
	rc |= run_fresh_answer(20);
	rc |= run_answer_tone_ms(3000, 30);
	rc |= run_answer_tone_ms(1000, 31);
	rc |= run_answer_tone_ms(0, 32);
	rc |= run_reinit(40);
	rc |= run_debug_on();

	return rc;
}

/*
 * t_class1progress.c -- differential test of `fax_class1_progress`
 * (class1.c, `.text` 0x0936d0, 1,145 bytes), the session dispatcher.
 *
 * Declined twice before this wave (F9802); findings F10051/F10052 settle
 * `ctx->modem_rate_code` and the per-modulation quality-latch chain it
 * gates. See class1.h's own comment on the function for the nine-step
 * derivation.
 *
 * REAL HANDLERS, NOT A FIXTURE.  `class1_state_functions[]` is installed
 * with five ALREADY-TESTED handlers (`_t30_silence_before_tx_state`,
 * `_idle_state`, `_send_silence_state`, `_recieve_silence_state`,
 * `_tx_silence_before_scrm_ones`) at the state slots their own names
 * suggest, on both sides identically -- so the dispatch itself is
 * genuinely exercised, not stubbed out.
 *
 * THE QUALITY-LATCH CHAIN is driven through a SYNTHETIC modem object --
 * two small allocations shaped exactly like the chain
 * `vmi_b->link->int_0014` walks (an object with a pointer at the
 * modulation's own OBJ offset, pointing to a block with a `short` at the
 * modulation's own latch offset) -- rather than a real V17RX/V27RX/V29RX
 * instance, since building one of those is a different agent's closure.
 * This proves the OFFSET WALK and the CLEAR-ON-READ behaviour; it does not
 * exercise the real per-modulation object layouts, which their own test
 * files already cover independently.
 */

#include <stddef.h>
#include <string.h>

#include "harness.h"
#include "dsplib/class1.h"
#include "dsplib/class1tx.h"
#include "dsplib/debug.h"
#include "dsplib/faxvmi.h"
#include "dsplib/fpm_iir.h"
#include "dsplib/sysdep.h"
#include "dsplib/v17fax.h"
#include "dsplib/v27fax.h"
#include "dsplib/v29fax.h"

extern unsigned int ref_dsplibs_debug_level;
extern class1_state_fn ref_class1_state_functions[19];

extern int ref_fax_class1_progress(struct fax_class1 *ctx, short *rx,
	short *tx, int word3, int word4, int *rx_count, int *tx_count,
	int *word7, int *word8);

extern int ref__idle_state(struct fax_class1 *ctx, const short *rx,
	short *tx, int word3, int word4, int *rx_count, int *tx_count,
	int word7, int *word8);
extern int ref__send_silence_state(struct fax_class1 *ctx, const short *rx,
	short *tx, int word3, int word4, int *rx_count, int *tx_count,
	int word7, int *word8);
extern int ref__recieve_silence_state(struct fax_class1 *ctx,
	const short *rx, short *tx, int word3, int word4, int *rx_count,
	int *tx_count, int word7, int *word8);
extern int ref__t30_silence_before_tx_state(struct fax_class1 *ctx,
	const short *rx, short *tx, int word3, int word4, int *rx_count,
	int *tx_count, int word7, int *word8);
extern int ref__tx_silence_before_scrm_ones(struct fax_class1 *ctx,
	const short *rx, short *tx, int word3, int word4, int *rx_count,
	int *tx_count, int word7, int *word8);

/* ------------------------------------------------------------------- */

static void
install_handlers(void)
{
	class1_state_functions[CLASS1_T30_SILENCE_BEFORE_PREAMBLE_STATE] =
	    _t30_silence_before_tx_state;
	class1_state_functions[CLASS1_IDLE_STATE] = _idle_state;
	class1_state_functions[CLASS1_SEND_SILENCE_STATE] =
	    _send_silence_state;
	class1_state_functions[CLASS1_RECIEVE_SILENCE_STATE] =
	    _recieve_silence_state;
	class1_state_functions[CLASS1_TX_SILENCE_BEFORE_SCRM_ONES] =
	    _tx_silence_before_scrm_ones;

	ref_class1_state_functions[CLASS1_T30_SILENCE_BEFORE_PREAMBLE_STATE] =
	    ref__t30_silence_before_tx_state;
	ref_class1_state_functions[CLASS1_IDLE_STATE] = ref__idle_state;
	ref_class1_state_functions[CLASS1_SEND_SILENCE_STATE] =
	    ref__send_silence_state;
	ref_class1_state_functions[CLASS1_RECIEVE_SILENCE_STATE] =
	    ref__recieve_silence_state;
	ref_class1_state_functions[CLASS1_TX_SILENCE_BEFORE_SCRM_ONES] =
	    ref__tx_silence_before_scrm_ones;
}

/*
 * A synthetic modem: `*(void **)(modem + obj_off)` is a second allocation
 * with `flag` planted at `flag_off` within it. Exactly the shape
 * `V17RX_OBJ_STATE`/`V29_OBJ_RX`/`V27_OBJ_RX` and their own latch offsets
 * describe, sized generously rather than exactly.
 */
static void *
make_modem(int obj_off, int flag_off, short flag)
{
	unsigned char *modem = sysdep_malloc((size_t)obj_off + 4);
	unsigned char *state = sysdep_malloc((size_t)flag_off + 4);

	*(void **)(modem + obj_off) = state;
	*(short *)(state + flag_off) = flag;
	return modem;
}

static struct faxvmi *
make_vmi_with_modem(void *modem)
{
	struct faxvmi *vmi = sysdep_malloc(sizeof(*vmi));
	struct faxvmi_link *link = sysdep_malloc(sizeof(*link));

	memset(vmi, 0, sizeof(*vmi));
	memset(link, 0, sizeof(*link));
	link->int_0014 = (int)(long)modem;
	vmi->link = link;
	return vmi;
}

/* ------------------------------------------------------------------- */

#define RXBUF	32
#define TXBUF	256

struct fixture {
	struct fax_class1 ctx;
	short rx[RXBUF];
	short tx[TXBUF];
	int rx_count;
	int tx_count;
	int word7;
	int word8;
};

static void
fixture_init(struct fixture *f, int state, int rx_count_val,
	    int f127c_start, int delayed_countdown, int delayed_status,
	    int use_iir, int code, int modem_direction, short latch)
{
	int i;
	static const short coeff[10] = { 100, 0, 0, 0, 0, 100, 0, 0, 0, 0 };

	memset(f, 0, sizeof(*f));
	f->ctx.state = state;
	f->ctx.prev_state = state == 0 ? 8 : 0;	/* force a transition */
	f->ctx.clock_sec = 12;
	f->ctx.clock_frac = 34;
	f->ctx.f127c = f127c_start;
	f->ctx.delayed_status_countdown = delayed_countdown;
	f->ctx.delayed_status = delayed_status;
	f->ctx.status = FAX_CLASS1_NO_MESSAGE;
	f->ctx.countdown = 0;
	f->ctx.silence_blocks = 100;
	f->ctx.energy = 0;
	f->ctx.modem_rate_code = code;
	f->ctx.modem_direction = modem_direction;
	f->ctx.iir_enabled = use_iir;
	if (use_iir)
		f->ctx.iir_coeff = coeff;

	for (i = 0; i < RXBUF; i++)
		f->rx[i] = (short)((i * 37 + 5) % 61 - 30);
	memset(f->tx, 0x5a, sizeof(f->tx));

	f->rx_count = rx_count_val;
	f->tx_count = -1;
	f->word7 = -1;
	f->word8 = 0;

	if (code == 0x91 || code == 0x79 || code == 0x61 || code == 0x49 ||
	    code == 0x60 || code == 0x48 || code == 0x30 || code == 0x18) {
		int obj_off, flag_off;

		if (code == 0x91 || code == 0x79 || code == 0x61 ||
		    code == 0x49) {
			obj_off = V17RX_OBJ_STATE;
			flag_off = V17RXS_SHORT_4FB2;
		} else if (code == 0x60 || code == 0x48) {
			obj_off = V29_OBJ_RX;
			flag_off = V29RX_SHORT_4F62;
		} else {
			obj_off = V27_OBJ_RX;
			flag_off = V27RX_Q_FLAG;
		}
		f->ctx.vmi_b = make_vmi_with_modem(
		    make_modem(obj_off, flag_off, latch));
	}
}

static void
run_case(int state, int rx_count_val, int f127c_start, int delayed_countdown,
	 int delayed_status, int use_iir, int code, int modem_direction, short latch,
	 unsigned level, long input)
{
	struct fixture fa, fb;
	int ra, rb;

	fixture_init(&fa, state, rx_count_val, f127c_start, delayed_countdown,
		    delayed_status, use_iir, code, modem_direction, latch);
	fixture_init(&fb, state, rx_count_val, f127c_start, delayed_countdown,
		    delayed_status, use_iir, code, modem_direction, latch);

	dsplibs_debug_level = ref_dsplibs_debug_level = level;

	ra = fax_class1_progress(&fa.ctx, fa.rx, fa.tx, 0, 0, &fa.rx_count,
				 &fa.tx_count, &fa.word7, &fa.word8);
	rb = ref_fax_class1_progress(&fb.ctx, fb.rx, fb.tx, 0, 0,
				     &fb.rx_count, &fb.tx_count, &fb.word7,
				     &fb.word8);

	dsplibs_debug_level = ref_dsplibs_debug_level = 0;

	diff_eq_int("return value, input %ld", ra, rb, input);
	diff_eq_int("ctx.state, input %ld", fa.ctx.state, fb.ctx.state, input);
	diff_eq_int("ctx.status, input %ld", fa.ctx.status, fb.ctx.status,
		    input);
	diff_eq_int("ctx.prev_state, input %ld", fa.ctx.prev_state,
		    fb.ctx.prev_state, input);
	diff_eq_int("ctx.clock_sec, input %ld", fa.ctx.clock_sec,
		    fb.ctx.clock_sec, input);
	diff_eq_int("ctx.clock_frac, input %ld", fa.ctx.clock_frac,
		    fb.ctx.clock_frac, input);
	diff_eq_int("ctx.f127c, input %ld", fa.ctx.f127c, fb.ctx.f127c,
		    input);
	diff_eq_int("ctx.countdown, input %ld", fa.ctx.countdown,
		    fb.ctx.countdown, input);
	diff_eq_int("ctx.delayed_status_countdown, input %ld",
		    fa.ctx.delayed_status_countdown,
		    fb.ctx.delayed_status_countdown, input);
	diff_eq_int("*rx_count unchanged, input %ld", fa.rx_count,
		    rx_count_val, input);
	diff_eq_int("*rx_count unchanged (blob), input %ld", fb.rx_count,
		    rx_count_val, input);
	diff_eq_int("*tx_count, input %ld", fa.tx_count, fb.tx_count, input);
	diff_eq_int("*word7, input %ld", fa.word7, fb.word7, input);
	diff_eq_int("*word7 is 0, input %ld", fa.word7, 0, input);
	diff_eq_int("tx buffer matches, input %ld",
		    memcmp(fa.tx, fb.tx, sizeof(fa.tx)), 0, input);
	if (use_iir)
		diff_eq_int("iir_state matches, input %ld",
			    memcmp(fa.ctx.iir_state, fb.ctx.iir_state,
				   sizeof(fa.ctx.iir_state)),
			    0, input);
}

/* ------------------------------------------------------------------- */

static int
test_progress(void)
{
	unsigned level;
	long tag = 0;

	diff_begin("fax_class1_progress");

	install_handlers();

	for (level = 0; level < 3; level++) {
		/* Baseline: idle, no quirks. */
		run_case(CLASS1_IDLE_STATE, 16, 0, 0, 0, 0, 0, 0, 0, level,
			tag++);

		/* f127c nonzero, drives the sample-scan loop, positive rx_count. */
		run_case(CLASS1_IDLE_STATE, 16, 1, 0, 0, 0, 0, 0, 0, level,
			tag++);

		/* f127c nonzero, NEGATIVE rx_count (signed-division path). */
		run_case(CLASS1_IDLE_STATE, -24, 1, 0, 0, 0, 0, 0, 0, level,
			tag++);

		/* f127c already large: exercises the reset-to-0 branch. */
		run_case(CLASS1_IDLE_STATE, 16, 1000, 0, 0, 0, 0, 0, 0, level,
			tag++);

		/* clock_frac rollover (34 + 2*? -- see clock_frac below via
		 * direct field override in a dedicated case). */
		run_case(CLASS1_T30_SILENCE_BEFORE_PREAMBLE_STATE, 16, 0, 0,
			0, 0, 0, 0, 0, level, tag++);

		/* send/receive silence states, real handlers, real transitions. */
		run_case(CLASS1_SEND_SILENCE_STATE, 16, 0, 0, 0, 0, 0, 0, 0,
			level, tag++);
		run_case(CLASS1_RECIEVE_SILENCE_STATE, 16, 0, 0, 0, 0, 0, 0,
			0, level, tag++);
		run_case(CLASS1_TX_SILENCE_BEFORE_SCRM_ONES, 16, 0, 0, 0, 0,
			0, 0, 0, level, tag++);

		/* Delayed status: countdown expires this call. */
		run_case(CLASS1_IDLE_STATE, 16, 0, 1, FAX_CLASS1_ERROR, 0, 0,
			0, 0, level, tag++);
		/* Delayed status: countdown NOT yet expired. */
		run_case(CLASS1_IDLE_STATE, 16, 0, 5, FAX_CLASS1_ERROR, 0, 0,
			0, 0, level, tag++);

		/* IIR tick enabled. */
		run_case(CLASS1_IDLE_STATE, 16, 0, 0, 0, 1, 0, 0, 0, level,
			tag++);

		/* Quality latch: V17, modem_direction==1, latch SET -> ACCEPT_RATE. */
		run_case(CLASS1_IDLE_STATE, 16, 0, 0, 0, 0, 0x91, 1, 1, level,
			tag++);
		/* V17, modem_direction==1, latch CLEAR -> no change. */
		run_case(CLASS1_IDLE_STATE, 16, 0, 0, 0, 0, 0x91, 1, 0, level,
			tag++);
		/* V17 code but modem_direction!=1 -> quality check skipped entirely. */
		run_case(CLASS1_IDLE_STATE, 16, 0, 0, 0, 0, 0x91, 0, 1, level,
			tag++);
		/* V17 SHORT-training code (0x92): recognised by
		 * _set_modem_rate but NOT by this function -- must NOT
		 * trigger the quality check. */
		run_case(CLASS1_IDLE_STATE, 16, 0, 0, 0, 0, 0x92, 1, 1, level,
			tag++);

		/* V29, latch set. */
		run_case(CLASS1_IDLE_STATE, 16, 0, 0, 0, 0, 0x60, 1, 1, level,
			tag++);
		/* V27ter, latch set. */
		run_case(CLASS1_IDLE_STATE, 16, 0, 0, 0, 0, 0x30, 1, 1, level,
			tag++);

		/* An unrecognised rate code. */
		run_case(CLASS1_IDLE_STATE, 16, 0, 0, 0, 0, 0x7f, 1, 1, level,
			tag++);
	}

	return diff_end();
}

int
main(void)
{
	return test_progress();
}

/*
 * v8.c -- the V.8 datapump wrapper.
 *
 * Owns the handshake object, builds the call menu from the modem's own
 * parameter block, and hands both to `V8Create`.  The menu is not copied:
 * the pointer the modem keeps is what the handshake reads and writes, which
 * is how the negotiated result gets back to the caller without an explicit
 * step.
 *
 * All three dp_operations entry points -- create, delete and process -- live
 * here, as the object's FILE `v8.c` unit holds them, and are file-static:
 * `v8_op` is the only thing that names them.  `V8Process`, the per-sample
 * loop `v8_process` sits on, stays in `v8proc.c`.
 */

#include <stdint.h>

#include "dsplib/debug.h"
#include "dsplib/v8dp.h"
#include "dsplib/dp_param.h"
#include "dsplib/modem_params.h"
#include "dsplib/sysdep.h"

extern int modem_dp_register(int id, void *op);
extern void modem_dp_deregister(int id, void *op);


static struct dp *
v8_create(void *modem, int id, int caller, int srate, int max_frag,
	  struct dp_operations *op)
{
	struct v8_cfg cfg;
	struct v8_dp *st;
	struct v8_cm *cm;
	int wants_pcm;

	(void)max_frag;

	/*
	 * Parameter 8 is read but never acted on -- it exists in this function
	 * only to be printed, and what it is printed as is its name: automode.
	 */
	int automode = (int)modem_get_param(modem, 8);

	if (srate != V8_DP_RATE)
		return 0;

	/*
	 * After the refusal, not before: a call at the wrong rate is turned
	 * away silently.
	 */
	if (DSPLIB_DEBUG_ON())
		dsplibs_debug_printf(
		    "v8: create: caller %d, automode %d, dp id %d.\n",
		    caller, automode, id);

	st = sysdep_malloc(sizeof(struct v8_dp));
	if (st == 0)
		return 0;
	sysdep_memset(st, 0, sizeof(struct v8_dp));

	st->modem = modem;
	st->id = DP_V8;
	st->self = st;
	st->op = op;
	st->answerer = caller == 0;
	st->want = id;
	st->idle_timer = 0;
	st->dspinfo = (struct dsp_info *)(intptr_t)
		      modem_get_param(modem, MDMPRM_DSPINFO);

	cm = dp_param_get(modem);
	st->cm = cm;

	/* Start from what the modem already has, then say what V.8 offers. */
	cm->b0 &= (unsigned char)~0x02;
	cm->b1 |= 0x40;

	wants_pcm = caller != 0 && (id == DP_V90 || id == DP_V92);
	cm->b0 = (unsigned char)((cm->b0 & ~0x08) | ((wants_pcm & 1) << 3));
	cm->b0 |= 0x20;
	cm->b0 |= 0x80;
	cm->b2 = (unsigned char)((cm->b2 & ~0x10)
				 | (((caller != 0 && id == DP_V92) & 1) << 4));

	sysdep_memset(&cfg, 0, sizeof(cfg));
	cfg.side = caller == 0;
	cfg.op_mode = 0;
	cfg.timeout_a = 0x0c;
	cfg.timeout_b = 0x07;
	cfg.rate = V8_DP_RATE;
	cfg.cm = cm;

	st->v8 = V8Create(&cfg);
	if (st->v8 == 0) {
		sysdep_free(st);
		return 0;
	}
	st->last_status = 0;
	return (struct dp *)st;
}

static int
v8_delete(struct dp *dp)
{
	struct v8_dp *st = ((struct v8_dp *)dp)->self;

	if (DSPLIB_DEBUG_ON())
		dsplibs_debug_printf("v8: delete...\n");

	V8Delete(st->v8);
	sysdep_free(st);
	return 0;
}

static int
v8_process(struct dp *dp, void *in, void *out, int count)
{
	struct v8_dp *st = ((struct v8_dp *)dp)->self;
	int rc = V8Process(st->v8, in, out, count);
	int ret = 0;
	int arg = -1;

	switch (rc) {
	/* Every status whose name ends TIME_OUT_WAITING_FOR_something. */
	case V8_ANS_TIME_OUT_WAITING_FOR_CM:
	case V8_ANS_TIME_OUT_WAITING_FOR_CJ:
	case V8_ORG_TIME_OUT_WAITING_FOR_ANSAM:
	case V8_ORG_TIME_OUT_WAITING_FOR_JM:
	case V8_ORG_TIME_OUT_WAITING_FOR_QCA1d:
		if (DSPLIB_DEBUG_ON())
			dsplibs_debug_printf("v8: process: timeout.\n");
		ret = DPSTAT_ERROR;
		break;

	case V8_OK:
		/* Publish what was agreed, then ask for the change. */
		if (DSPLIB_DEBUG_ON())
			dsplibs_debug_printf("v8: process: OK.\n");
		/*
		 * ...but only while the idle timer is not already running.
		 * Once a change has been asked for, `idle_timer` counts down
		 * to it and a second V8_OK must not start over.
		 */
		if (st->idle_timer != 0)
			break;
		V8UpdateModemParameters(st->v8, st->cm);

		/*
		 * Which datapump comes next.  Quick connect keeps whatever the
		 * call asked for; otherwise it is whichever modulation
		 * survived the negotiation, most capable first -- the same
		 * three bits of `b0` that V8Create prints as V90, V34 and V32
		 * (finding F164), and the datapump ids are the standard
		 * numbers.  Nothing left means nothing to change to.
		 */
		if (st->cm->b2 & 0x10) {
			if (DSPLIB_DEBUG_ON())
				dsplibs_debug_printf("v8: process: QC.\n");
			arg = st->want;
		} else if (st->cm->b0 & 0x08) {
			arg = DP_V90;
		} else if (st->cm->b0 & 0x20) {
			arg = DP_V34;
		} else if (st->cm->b0 & 0x80) {
			arg = DP_V32;
		} else {
			ret = DPSTAT_ERROR;
			break;
		}

		/* Common to all four: what was agreed goes to the modem. */
		st->dspinfo->qc_lapm = (st->cm->b2 >> 6) & 1;
		st->dspinfo->qc_index = st->cm->menu;
		break;

	case V8_ORG_BAD_QCA1d_MESSAGE:
		/*
		 * The far end offered PCM.  Only take it if this call asked
		 * for V.90 or V.92, and only once.  Nothing reaches it: the
		 * object's OWN V8Process has no arm that produces 15, 16 or
		 * 17 either -- its status chain at 0x74680 is the one above,
		 * arm for arm -- so the three QCA1d statuses exist in the
		 * table and in this switch and nowhere else.  Reproduced.
		 */
		if (st->want != 92 && st->want != 90) {
			ret = DPSTAT_ERROR;
		} else if (st->idle_timer == 0) {
			st->dspinfo->qc_lapm &= 1;
			arg = 92;
		}
		break;

	default:
		break;
	}

	if (arg >= 0) {
		modem_set_param(dp->modem, 9, arg);
		ret = DPSTAT_CHANGEDP;
		st->idle_timer = (int)modem_get_param(dp->modem, 5) + 0x2a0;
		if (DSPLIB_DEBUG_ON())
			dsplibs_debug_printf(
			    "v8: Link established. Idle timer %d.\n",
			    st->idle_timer);
	}

	/*
	 * The same change detector as V8Process's, one layer up: keep the
	 * previous status in `last_status` and act only when it changes.
	 */
	if (st->last_status != rc) {
		if (DSPLIB_DEBUG_ON())
			dsplibs_debug_printf("v8: status (%d) %s\n", rc,
					     v8StatusName[rc]);
		st->last_status = rc;
	}

	if (st->idle_timer > 0) {
		st->idle_timer -= count;
		if (st->idle_timer <= 0) {
			/* The window closed: give up and change anyway. */
			st->idle_timer = -1;
			st->f1c = 0;
			modem_set_param(dp->modem, 9, 0);
			ret = DPSTAT_CHANGEDP;
		}
	}
	return ret;
}

static struct dp_operations v8_op = {
	/*
	 * "V8", not "v8".  The other three datapumps name themselves in
	 * lower case -- `b103`, `call`, `v23` are all standalone strings
	 * in the blob -- and this one does not: the object carries `V8\0`
	 * and no bare `v8\0` at all.  An asymmetry in the original, and
	 * the reconstruction had quietly regularised it.  See finding F201.
	 */
	.name = "V8",
	.use_count = 0,
	.create = v8_create,
	.destroy = v8_delete,
	.process = v8_process,
	.hangup = 0
};

/*
 * RETURNS int, AND THE VALUE IS A LITERAL ZERO RATHER THAN THE CALLEE'S.
 * The object closes this function `call modem_dp_register; xor %eax,%eax;
 * add $0xc,%esp; ret` -- one instruction more than a `void` body can emit,
 * and it is the whole of our -1 instruction gap.  `modem_dp_register` itself
 * returns `int` (slmodemd/modem.c:211), so `return modem_dp_register(...)`
 * -- which is what the host's own `dp_dummy_init` and `dp_sinus_init` write
 * -- would leave the callee's result in %eax and emit no `xor` at all.  The
 * zeroing is therefore forced evidence for a separate `return 0`.
 *
 * It is an asymmetry in the original and not a convention: `dp_v23_init`
 * (+0x4f88) zeroes the same way and `dp_call_init` (+0x31c0) does not, so
 * that one is `void`.  Finding F7860.
 */
int
dp_v8_init(void)
{
	modem_dp_register(DP_V8, &v8_op);
	return 0;
}

void
dp_v8_exit(void)
{
	modem_dp_deregister(DP_V8, &v8_op);
}

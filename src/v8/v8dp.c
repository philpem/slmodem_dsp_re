/*
 * v8dp.c -- the V.8 datapump wrapper.
 *
 * Owns the handshake object, builds the call menu from the modem's own
 * parameter block, and hands both to `V8Create`.  The menu is not copied:
 * the pointer the modem keeps is what the handshake reads and writes, which
 * is how the negotiated result gets back to the caller without an explicit
 * step.
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
	st->f20 = 0;
	st->dspinfo = (struct v8_dspinfo *)(intptr_t)
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
	cfg.f10 = V8_DP_RATE;
	cfg.cm = cm;

	st->v8 = V8Create(&cfg);
	if (st->v8 == 0) {
		sysdep_free(st);
		return 0;
	}
	st->f2c = 0;
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

static struct dp_operations v8_op = {
	.name = "v8",
	.use_count = 0,
	.create = v8_create,
	.destroy = v8_delete,
	.process = v8_process,
	.hangup = 0
};

void
dp_v8_init(void)
{
	modem_dp_register(DP_V8, &v8_op);
}

void
dp_v8_exit(void)
{
	modem_dp_deregister(DP_V8, &v8_op);
}

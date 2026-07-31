/*
 * v8neg.c -- one end of a V.8 call, for the interop tier.  See v8neg.h.
 */

#include <stdio.h>
#include <string.h>

#include "v8neg.h"

/* The reconstruction's own entry points. */
const struct v8_ops v8neg_ours = {
	.create		= V8Create,
	.destroy	= V8Delete,
	.process	= V8Process,
	.get_message	= V8GetMessage,
	.update		= V8UpdateModemParameters,
	.rc_create	= RcFixed_Create,
	.rc_destroy	= RcFixed_Delete,
	.rc_resample	= RcFixed_Resample,
};

/*
 * The menu this end offers.  See v8neg.h for what the bits mean.
 *
 * Bit 4 of b2 is deliberately clear.  That is the PCM offer, and setting it
 * switches the handshake onto Smart Link's own six-word QCA1 exchange, which
 * is not in V.8 and which no independent implementation can answer.
 */
static void
init_menu(struct v8_cm *cm, unsigned char b0, unsigned char b1)
{
	memset(cm, 0, sizeof(*cm));
	cm->b0 = b0;
	cm->b1 = b1;
	cm->b2 = 0x00;
	cm->menu = 0;
}

int
side_create(struct side *s, const struct v8_ops *ops, int mode,
	    unsigned char b0, unsigned char b1)
{
	struct v8_cfg cfg;

	memset(s, 0, sizeof(*s));
	s->ops = ops;
	init_menu(&s->cm, b0, b1);

	memset(&cfg, 0, sizeof(cfg));
	cfg.mode = mode;
	cfg.f04 = 0;
	/*
	 * Both deadlines in seconds.  They have to outlast the whole
	 * exchange: expiring is how the handshake reports "the far end never
	 * answered", and a negotiation that took longer than the deadline
	 * would look like a fault in the reconstruction rather than a slow
	 * test.
	 */
	cfg.timeout_a = 30;
	cfg.timeout_b = 30;
	cfg.f10 = 0;
	cfg.cm = &s->cm;

	/*
	 * V8Create does not zero the object -- only the six configuration
	 * words and whatever v8handshakinit writes are defined -- so nothing
	 * here may read a field neither of those touched.
	 */
	s->v8 = ops->create(&cfg);
	s->up = ops->rc_create(RC_UP);
	s->down = ops->rc_create(RC_DOWN);
	s->status = -1;
	s->best = -1;
	return s->v8 != NULL && s->up != NULL && s->down != NULL;
}

void
side_delete(struct side *s)
{
	s->ops->destroy(s->v8);
	s->ops->rc_destroy(s->up);
	s->ops->rc_destroy(s->down);
	s->v8 = NULL;
	s->up = NULL;
	s->down = NULL;
}

int
side_frame(struct side *s, const short *in, int n_in, short *out, int out_max)
{
	short native_in[V8NEG_NATIVE];
	short native_out[V8NEG_NATIVE];
	int n_native = V8NEG_NATIVE;
	int n_out = out_max;
	int st;

	if (n_in > V8NEG_FRAME)
		n_in = V8NEG_FRAME;

	s->ops->rc_resample(s->up, in, n_in, native_in, &n_native);
	if (n_native <= 0)
		return 0;

	st = s->ops->process(s->v8, native_in, native_out, n_native);
	s->status = st;
	if (st > s->best)
		s->best = st;
	if (st == V8NEG_STATUS_DONE)
		s->negotiated = 1;

	s->ops->rc_resample(s->down, native_out, n_native, out, &n_out);
	return n_out;
}

int
side_check(struct side *s, const char *who, const struct v8neg_expect *e)
{
	unsigned char msg[32];
	int count = (int)sizeof(msg);
	int rc;
	int i;
	int ok;

	rc = s->ops->get_message(s->v8, msg, &count);
	printf("    %s: received", who);
	if (rc == V8_GET_EMPTY) {
		printf(" nothing\n");
		return 0;
	}
	for (i = 0; i < count; i++)
		printf(" %02x", msg[i]);
	printf("%s\n", rc > 0 ? " (truncated)" : "");

	/*
	 * And what that comes to once intersected with what this end asked
	 * for.  This is the step the datapump layer takes on status 13.
	 */
	s->ops->update(s->v8, &s->cm);
	printf("    %s: agreed b0=%02x b1=%02x b2=%02x ->", who,
	       s->cm.b0, s->cm.b1, s->cm.b2);
	if (s->cm.b0 & 0x80)
		printf(" V.32");
	if (s->cm.b0 & 0x20)
		printf(" V.34");
	if (s->cm.b1 & 0x20)
		printf(" V.21");
	if (s->cm.b1 & 0x10)
		printf(" V.23");
	if (s->cm.b1 & 0x40)
		printf(" (V-series call)");
	printf("\n");

	/*
	 * A menu octet misread in either direction shows up here, and this is
	 * exactly what the differential harness cannot see: agreeing with the
	 * blob about a wrong bit looks the same as agreeing about a right one.
	 */
	ok = (s->cm.b0 & e->b0_set) == e->b0_set
	     && (s->cm.b0 & e->b0_clear) == 0
	     && (s->cm.b1 & e->b1_set) == e->b1_set
	     && (s->cm.b1 & e->b1_clear) == 0;
	if (!ok)
		printf("    %s: expected b0 +%02x -%02x, b1 +%02x -%02x\n",
		       who, e->b0_set, e->b0_clear, e->b1_set, e->b1_clear);
	return ok;
}

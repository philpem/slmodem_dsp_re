/*
 * v8neg.c -- one end of a V.8 call, for the interop tier.  See v8neg.h.
 */

#include <stdio.h>
#include <string.h>

#include "v8neg.h"

/*
 * The menu this end offers.
 *
 * The flag bytes are what `initTxSequence` reads, and each maps to one bit of
 * one V.8 octet: b0 bit 5 is V.34, b0 bit 7 is V.32, b1 bit 4 is V.23, b1 bit
 * 5 is V.21, and b1 bit 6 asks for the V-series call function.
 *
 * Bit 4 of b2 is deliberately clear.  That is the PCM offer, and setting it
 * switches the handshake onto Smart Link's own six-word QCA1 exchange, which
 * is not in V.8 and which no independent implementation can answer.
 */
static void
init_menu(struct v8_cm *cm)
{
	memset(cm, 0, sizeof(*cm));
	cm->b0 = 0x20 | 0x80;
	cm->b1 = 0x40 | 0x20 | 0x10;
	cm->b2 = 0x00;
	cm->menu = 0;
}

/* What the menu above comes to, as SpanDSP names the modulations. */
#define OFFERED	(V8_MOD_V21 | V8_MOD_V23 | V8_MOD_V32 | V8_MOD_V34)

void
v8neg_spandsp_parms(v8_parms_t *parms)
{
	memset(parms, 0, sizeof(*parms));
	parms->modem_connect_tone = MODEM_CONNECT_TONES_ANSAM_PR;
	parms->send_ci = false;
	parms->v92 = -1;
	parms->jm_cm.call_function = V8_CALL_V_SERIES;
	parms->jm_cm.modulations = OFFERED;
	parms->jm_cm.protocols = V8_PROTOCOL_LAPM_V42;
}

int
side_create(struct side *s, int mode)
{
	struct v8_cfg cfg;

	memset(s, 0, sizeof(*s));
	init_menu(&s->cm);

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
	s->v8 = V8Create(&cfg);
	s->up = RcFixed_Create(RC_UP);
	s->down = RcFixed_Create(RC_DOWN);
	s->status = -1;
	s->best = -1;
	return s->v8 != NULL && s->up != NULL && s->down != NULL;
}

void
side_delete(struct side *s)
{
	V8Delete(s->v8);
	RcFixed_Delete(s->up);
	RcFixed_Delete(s->down);
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

	RcFixed_Resample(s->up, in, n_in, native_in, &n_native);
	if (n_native <= 0)
		return 0;

	st = V8Process(s->v8, native_in, native_out, n_native);
	s->status = st;
	if (st > s->best)
		s->best = st;
	if (st == V8NEG_STATUS_DONE)
		s->negotiated = 1;

	RcFixed_Resample(s->down, native_out, n_native, out, &n_out);
	return n_out;
}

const char *
v8neg_status_name(int st)
{
	switch (st) {
	case V8_STATUS_IN_PROGRESS:	return "in progress";
	case V8_STATUS_V8_OFFERED:	return "V.8 offered";
	case V8_STATUS_V8_CALL:		return "V.8 negotiated";
	case V8_STATUS_NON_V8_CALL:	return "not a V.8 call";
	case V8_STATUS_FAILED:		return "failed";
	case V8_STATUS_CALL_FUNCTION_RECEIVED: return "call function seen";
	case V8_STATUS_CALLING_TONE_RECEIVED: return "calling tone";
	case V8_STATUS_FAX_CNG_TONE_RECEIVED: return "CNG";
	default:			return "nothing yet";
	}
}

int
side_report(struct side *s, const char *who)
{
	unsigned char msg[32];
	int count = (int)sizeof(msg);
	int rc;
	int i;
	int ok;

	rc = V8GetMessage(s->v8, msg, &count);
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
	V8UpdateModemParameters(s->v8, &s->cm);
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
	 * Both ends offered the same four, so the intersection has to be all
	 * four.  Anything less means a menu octet was misread in one
	 * direction or the other -- which is exactly what the differential
	 * harness cannot see, because agreeing with the blob about a wrong
	 * bit looks the same as agreeing about a right one.
	 */
	ok = (s->cm.b0 & 0xa0) == 0xa0 && (s->cm.b1 & 0x70) == 0x70;
	return ok;
}

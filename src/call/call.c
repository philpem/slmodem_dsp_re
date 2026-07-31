/*
 * call.c -- the call-setup datapump.
 *
 * Registered as `DP_CALL`, this is what runs between "go off hook" and "we
 * have a carrier".  All the judgement lives in `CALLPROG_Progress`; this file
 * dials, feeds it 48 samples at a time whatever size the host asks in, and
 * turns the message it returns into a datapump status.
 *
 * The size mismatch is the reason for most of the code.  The host hands over
 * arbitrary counts, `CALLPROG_Progress` wants fixed blocks, and when the host
 * is not running at 8 kHz there is a resampler in each direction as well.  So
 * each direction has a ring the host's samples flow through and a 96-byte
 * double buffer the supervisor works in, and the two halves swap every block.
 */

#include <stdint.h>

#include "dsplib/call.h"
#include "dsplib/dp.h"
#include "dsplib/callprog.h"
#include "dsplib/callprog_state.h"
#include "dsplib/fixedrc.h"
#include "dsplib/modem_params.h"
#include "dsplib/sysdep.h"

extern long modem_get_sreg(void *modem, unsigned int num);
extern int modem_dp_register(int id, void *op);

/* The rate the supervisor works at; anything else needs resampling. */
#define CALL_NATIVE_RATE	8000

/*
 * Which resampler design converts which host rate.  `RcFixed_Create` takes a
 * mode rather than a ratio, so these are the four the original names.
 */
#define RC_9600_IN		3
#define RC_9600_OUT		2
#define RC_48000_IN		5
#define RC_48000_OUT		4

/*
 * The supervisor asks for S-registers through a callback rather than calling
 * `modem_get_sreg` itself, because it has no idea what a modem is.  This is
 * the whole of the adaptor: widen the register number and tail-call.
 */
static long
call_GetSRegister(void *modem, unsigned short num)
{
	return modem_get_sreg(modem, num);
}

/*
 * Build the dial string the supervisor will be given.
 *
 * `MDMPRM_DIALSTR` holds what the host asked for.  If it starts with a digit
 * it carries no mode prefix, so one is put in front of it -- `p` or `t`
 * according to S16 -- into the caller's buffer.  Anything else (a `T`, a `P`,
 * a modifier) is passed through untouched, and so is anything too long to
 * prefix.
 *
 * Returns the string to dial, which is either `buf` or the parameter itself.
 */
static const char *
call_dial_string(struct call_dp *st, char *buf, size_t buflen)
{
	const char *want = (const char *)(intptr_t)
			   modem_get_param(st->modem, MDMPRM_DIALSTR);

	(void)buflen;

	if ((unsigned char)(*want - '0') > 9)
		return want;
	/*
	 * One byte for the prefix and one for the terminator, against a
	 * 64-byte buffer.
	 */
	if (sysdep_strlen(want) > 62)
		return want;

	buf[0] = modem_get_sreg(st->modem, 16) < 1 ? 'p' : 't';
	sysdep_strcpy(buf + 1, want);
	return buf;
}

static struct dp *
call_create(void *modem, int id, int caller, int srate, int max_frag,
	    struct dp_operations *op)
{
	struct callprog_cfg cfg;
	char dialstr[64];
	struct call_dp *st;
	int mode;

	(void)caller;
	(void)max_frag;

	st = sysdep_malloc(sizeof(struct call_dp));
	if (st == 0)
		return 0;
	sysdep_memset(st, 0, sizeof(struct call_dp));

	st->modem = modem;
	st->self = st;
	st->id = id;
	st->op = op;

	/*
	 * S56 chooses how hard to listen.  Its exact meaning is the host's
	 * business; all that matters here is that 0, 1 and 3 mean one thing
	 * and everything else means the other.
	 */
	mode = modem_get_sreg(modem, 56);
	cfg.w0 = (mode <= 1 || mode == 3) ? 1 : 0;
	cfg.get_sreg = call_GetSRegister;
	cfg.modem = st->modem;
	cfg.w3 = 0;

	st->pulse_break = modem_get_param(modem, GetPulseDialBreakTime);
	st->pulse_make = modem_get_param(modem, GetPulseDialMakeTime);
	st->f5bc = 0;

	CALLPROG_Create(&st->callprog, &cfg);
	CALLPROG_Dial(&st->callprog, call_dial_string(st, dialstr,
						      sizeof(dialstr)));
	/*
	 * One past the last real message, as "nothing reported yet": the
	 * first block overwrites it, and until then the comparison against
	 * the previous message can never match.
	 */
	st->message = CALLPROG_MAX_MESSAGES;

	/*
	 * At 8 kHz the supervisor sees the host's samples directly.  At any
	 * other rate both directions go through a fixed-ratio converter, and
	 * a rate with no converter simply gets none -- the pointers stay
	 * null and the block path treats that as 8 kHz.
	 */
	if (srate != CALL_NATIVE_RATE) {
		st->rc_in = srate == 9600 ? RcFixed_Create(RC_9600_IN)
			  : srate == 48000 ? RcFixed_Create(RC_48000_IN) : 0;
		st->rc_out = srate == 9600 ? RcFixed_Create(RC_9600_OUT)
			   : srate == 48000 ? RcFixed_Create(RC_48000_OUT) : 0;
	}

	/*
	 * Prime the outgoing side with one block of silence, and point the
	 * supervisor at the other half of the double buffer, so that the
	 * first call has something to send before it has processed anything.
	 */
	st->out_q.count = CALL_BLOCK_BYTES;
	st->out_q.active = CALL_BLOCK_BYTES;

	return (struct dp *)st;
}

static int
call_delete(struct dp *dp)
{
	struct call_dp *st = ((struct call_dp *)dp)->self;

	CALLPROG_Delete(&st->callprog);
	if (st->rc_in != 0)
		RcFixed_Delete(st->rc_in);
	if (st->rc_out != 0)
		RcFixed_Delete(st->rc_out);
	st->self = 0;
	sysdep_free(st);
	return 0;
}

/*
 * Turn the supervisor's message into a datapump status.  Everything that
 * means "somebody answered" also latches `answered`, after which the incoming
 * block is thrown away rather than listened to: the line belongs to whatever
 * datapump takes over next.
 */
static int
call_status(struct call_dp *st, int message)
{
	switch (message) {
	case CALLPROG_NO_RING:
	case CALLPROG_NO_ANSWER:
	case CALLPROG_ANSWER_STATE_TIMEOUT:
		return DPSTAT_NOANSWER;

	case CALLPROG_NO_DIAL_TONE:
		return DPSTAT_NODIALTONE;

	case CALLPROG_BUSY:
	case CALLPROG_CONGESTION:
		return DPSTAT_BUSY;

	case CALLPROG_ERROR:
		return DPSTAT_ERROR;

	case CALLPROG_ANSWER:
	case CALLPROG_MODEM_ANSWER:
	case CALLPROG_VOICE_ANSWER:
	case CALLPROG_V8BIS_MODEM_ANSWER:
	case CALLPROG_DUALTONE_A:
	case CALLPROG_DUALTONE_B:
		st->answered = 1;
		return DPSTAT_CHANGEDP;

	default:
		/*
		 * Including CALLPROG_DIALING, CALLPROG_END_DIALING and
		 * CALLPROG_RINGBACK: progress worth reporting to a human, but
		 * nothing for the host to do about it.
		 */
		return DPSTAT_OK;
	}
}

/* One block: 48 samples in, 48 out, through however many stages there are. */
static int
call_block(struct call_dp *st)
{
	short *from_line = st->in_q.data + st->in_q.active / 2;
	short *to_line = st->out_q.data + st->out_q.active / 2;
	const short *in;
	short *out;
	int count;
	int rc;

	if (st->answered != 0) {
		/* The call is over; keep the outgoing side quiet. */
		sysdep_memset(to_line, 0, CALL_BLOCK_BYTES);
		return DPSTAT_OK;
	}

	if (st->rc_in != 0) {
		count = CALL_SCRATCH_SAMPLES;
		RcFixed_Resample(st->rc_in, from_line, CALL_BLOCK_SAMPLES,
				 st->from_host, &count);
		in = st->from_host;
		out = st->to_host;
	} else {
		count = CALL_BLOCK_SAMPLES;
		in = from_line;
		out = to_line;
	}

	st->message = CALLPROG_Progress(&st->callprog, in, out, count);

	if (st->rc_out != 0) {
		int produced = CALL_SCRATCH_SAMPLES;

		RcFixed_Resample(st->rc_out, st->to_host, count, to_line,
				 &produced);
	}

	rc = call_status(st, st->message);
	return rc;
}

static int
call_run(struct dp *dp, void *in, void *out, int count)
{
	struct call_dp *st = ((struct call_dp *)dp)->self;
	char *host_in = in;
	char *host_out = out;
	int rc = 0;

	while (count > 0) {
		int n;

		/*
		 * How many bytes to move this time: a whole block, unless the
		 * host asked for less, and never past the end of either ring.
		 */
		n = count > CALL_BLOCK_SAMPLES - 1 ? CALL_BLOCK_BYTES
						   : count * 2;
		if (n > CALL_RING_BYTES - st->in_q.head)
			n = CALL_RING_BYTES - st->in_q.head;
		if (n > CALL_RING_BYTES - st->out_q.tail)
			n = CALL_RING_BYTES - st->out_q.tail;

		sysdep_memcpy((char *)st->in_q.data + st->in_q.head, host_in,
			      n);
		st->in_q.count += n;
		st->in_q.head = (st->in_q.head + n) % CALL_RING_BYTES;
		host_in += n;

		if (st->in_q.count > CALL_BLOCK_BYTES - 1) {
			int block = call_block(st);

			if (block != DPSTAT_OK)
				rc = block;

			st->in_q.count -= CALL_BLOCK_BYTES;
			st->out_q.count += CALL_BLOCK_BYTES;
			/*
			 * Swap both halves of the double buffer.  The test is
			 * against 1 rather than 0 because these hold a byte
			 * offset of 0 or 96, never anything between.
			 */
			st->in_q.active = st->in_q.active < 1
					  ? CALL_BLOCK_BYTES : 0;
			st->out_q.active = st->out_q.active < 1
					   ? CALL_BLOCK_BYTES : 0;
		}

		sysdep_memcpy(host_out,
			      (char *)st->out_q.data + st->out_q.tail, n);
		host_out += n;
		st->out_q.count -= n;
		st->out_q.tail = (st->out_q.tail + n) % CALL_RING_BYTES;
		count -= n / 2;
	}

	return rc;
}

/*
 * The operations table.  A file static in the original, and the only thing
 * that refers to the four functions above -- which is why they can be static
 * too, and why a test has to come in through `dp_call_init`.
 */
static struct dp_operations call_op = {
	.name = "call",
	.use_count = 0,
	.create = call_create,
	.destroy = call_delete,
	.process = call_run,
	.hangup = 0,
};

void
dp_call_init(void)
{
	modem_dp_register(DP_CALL, &call_op);
}

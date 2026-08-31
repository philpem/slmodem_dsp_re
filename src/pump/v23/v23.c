/*
 * v23.c -- ITU-T V.23: datapump registration and glue.
 *
 * Reconstructed from dsplibs.o v23.c:
 *   v23_create   .text 0x004c30   274 bytes   (file-local)
 *   v23_delete   .text 0x004d50    72 bytes   (file-local)
 *   v23_process  .text 0x004da0   464 bytes   (file-local)
 *   dp_v23_init  .text 0x004f70    30 bytes
 *   dp_v23_exit  .text 0x004f90    28 bytes
 *   v23_ops      .data  0x000060    24 bytes
 *
 * The thin layer between the modem core and the V.23 modulation, and it is
 * b103.c with three constants changed -- same shape, same ops table, same
 * dp_wrapper in front of the same 160-sample 8 kHz fragment, the same trick
 * of widening one bit buffer in place rather than keeping two.  Reading the
 * two side by side is the fastest way to see what belongs to V.23 and what
 * belongs to the datapump architecture.
 *
 * ---------------------------------------------------------------------------
 * `caller` decides everything, through one comparison
 *
 * One argument selects which end of the call this is, and v23_create uses it
 * twice in the same expression:
 *
 *     mode        = (caller == 0)
 *     answer_tone = (caller == 0)
 *
 * So the station that did NOT place the call is the host: it answers with the
 * 2100 Hz tone, transmits the 1200 bps forward channel and listens on the
 * 75 bps backward one.  The station that placed the call is the terminal and
 * does the opposite.  Note the polarity -- `caller` non-zero means this
 * station originated -- which is the same inversion b103_create's `sete`
 * carries and for the same reason.
 *
 * ---------------------------------------------------------------------------
 * How many bits to ask for
 *
 * Bell 103 asks the core for a fixed six bits a block.  V.23 cannot: the two
 * directions run at rates sixteen times apart, and neither is a whole number
 * of bits per 160-sample frame.  So this file asks for whatever the
 * transmitter reported it CONSUMED last time.  V23ModemMain's `tx_nbits` is
 * in/out, and its output becomes the next block's request:
 *
 *     n_tx = self->tx_bits_wanted;      // last block's answer
 *     V23ModemMain(..., &n_tx, ...);    // overwritten with what it used
 *     self->tx_bits_wanted = n_tx;      // next block's request
 *
 * It starts at zero, from the memset, so no data is fetched until the
 * transmitter has run once -- and the fetch is gated on `connected` as well,
 * so nothing is fetched at all until carrier is up.  Both together are what
 * keeps the core's data off a line that is still training.
 */

#include <stddef.h>

#include "dsplib/dp_wrapper.h"
#include "dsplib/modem_params.h"
#include "dsplib/sysdep.h"
#include "dsplib/v23.h"

/* Provided by the modem core (slmodemd/modem.c). */

extern int modem_dp_register(int id, void *op);
extern void modem_dp_deregister(int id, void *op);
extern int modem_get_bits(void *modem, int chan, unsigned char *bits,
			  unsigned short count);
extern int modem_put_bits(void *modem, int chan, const unsigned char *bits,
			  unsigned short count);

/* FILE-LOCAL, on the same evidence as b103.c's `b103_ops`.  Finding F8121. */
static struct dp_operations v23_ops = {
	.name = "v23",
	.create = v23_create,
	.destroy = v23_delete,
	/*
	 * `process` is dp_wrapper_run, NOT v23_process -- see b103.c for what
	 * that says about the architecture.  v23_process reaches the table
	 * only by having been handed to dp_wrapper_create.
	 */
	.process = (int (*)(struct dp *, void *, void *, int))dp_wrapper_run
	/* use_count and hangup are zero */
};

int
dp_v23_init(void)
{
	modem_dp_register(DP_V23, &v23_ops);
	return 0;
}

void
dp_v23_exit(void)
{
	modem_dp_deregister(DP_V23, &v23_ops);
}

/*
 * ---------------------------------------------------------------------------
 * v23_create -- .text 0x004c30.
 *
 * The configuration is three fields and every one of them is a constant here
 * except the answer-tone flag.  8000 is the datapump's own rate, so the
 * answer-tone sequence is timed against the rate the modulation actually
 * runs at rather than against whatever the host asked for; 700 is the
 * carrier-loss timeout in milliseconds, which both receivers charge at 20 per
 * block -- 35 blocks, 700 ms of dead line before the call is dropped.
 */
struct dp *
v23_create(void *modem, int id, int caller, int srate, int max_frag,
	   struct dp_operations *op)
{
	struct v23_cfg cfg;
	struct v23_dp *dp;

	(void)max_frag;

	/* The original prints "v23: create...\n" here at debug level 2. */

	dp = (struct v23_dp *)sysdep_malloc(sizeof(*dp));
	if (dp == NULL)
		return NULL;
	sysdep_memset(dp, 0, sizeof(*dp));

	dp->caller = caller;
	dp->dp.id = id;
	dp->dp.modem = modem;
	dp->dp.op = op;

	dp->wrapper = dp_wrapper_create(dp, v23_process, V23_DP_FRAG, srate,
					V23_DP_SRATE);
	if (dp->wrapper == NULL) {
		sysdep_free(dp);
		return NULL;
	}
	dp->dp.dp_data = dp->wrapper;
	dp->wrapper->dp = &dp->dp;

	/*
	 * `cfg.r01` is left uninitialised, as the original leaves it: three
	 * bytes of stack that nothing downstream reads.
	 */
	cfg.answer_tone = (unsigned char)(caller == 0);
	cfg.sample_rate = V23_DP_SRATE;
	cfg.silence_limit = V23_SILENCE_MS;

	dp->modem = CreateV23Modem(NULL, caller == 0, &cfg);
	if (dp->modem == NULL) {
		dp_wrapper_delete(dp->wrapper);
		sysdep_free(dp);
		return NULL;
	}

	return &dp->dp;
}

/*
 * v23_delete -- .text 0x004d50.
 *
 * Reaches its own state out through `dp_data` and back through the wrapper's
 * back-pointer, landing exactly where it started.  Same as b103_delete, and
 * reproduced for the same reason.
 */
int
v23_delete(struct dp *dp)
{
	struct v23_dp *self = (struct v23_dp *)
		((struct dp_wrapper *)dp->dp_data)->dp;

	DeleteV23Modem(self->modem);
	dp_wrapper_delete(self->wrapper);
	dp->dp_data = NULL;
	sysdep_free(self);
	return 0;
}

/*
 * ---------------------------------------------------------------------------
 * v23_process -- .text 0x004da0.
 *
 * One block, called by dp_wrapper at the datapump's own rate.
 *
 * ---------------------------------------------------------------------------
 * The status translation
 *
 * V23ModemMain returns the receiver's view of the line, and this is where it
 * becomes a DPSTAT_*:
 *
 *     0  carrier up   -> DPSTAT_CONNECT on the first such block,
 *                        DPSTAT_OK thereafter
 *     1  acquiring    -> DPSTAT_OK
 *     2  given up     -> DPSTAT_ERROR
 *
 * `connected` is the edge detector.  Note that 1 and 2 both CLEAR it, so a
 * receiver that loses and regains carrier reports a second DPSTAT_CONNECT and
 * sets the line rates again -- which is harmless, and is not how b103.c does
 * it (there the edge is taken against `dp->status`).
 */
int
v23_process(void *dp_arg, void *in, void *out, int count)
{
	struct dp *dp = (struct dp *)dp_arg;
	struct v23_dp *self = (struct v23_dp *)
		((struct dp_wrapper *)dp->dp_data)->dp;
	int n_tx = self->tx_bits_wanted;
	int n_rx = 0;
	int status;
	int i;

	if (self->connected != 0 && n_tx > 0) {
		/*
		 * Fetch as bytes into the same buffer V23ModemMain will read
		 * as ints, then widen BACKWARDS -- writing element i as an int
		 * cannot clobber element i+1 as a byte if you start from the
		 * end.
		 */
		n_tx = modem_get_bits(dp->modem, 1,
				      (unsigned char *)self->tx_bits,
				      (unsigned short)n_tx);
		for (i = n_tx - 1; i >= 0; i--)
			self->tx_bits[i] =
				((unsigned char *)self->tx_bits)[i] & 1;
	}

	status = V23ModemMain(self->modem, self->tx_bits, &n_tx, (short *)out,
			      count, (short *)in, count, self->rx_bits, &n_rx);

	if (self->connected != 0 && n_rx > 0) {
		for (i = 0; i < n_rx; i++)
			((unsigned char *)self->rx_bits)[i] =
				(unsigned char)(self->rx_bits[i] & 1);
		modem_put_bits(dp->modem, 1, (unsigned char *)self->rx_bits,
			       (unsigned short)n_rx);
	}

	self->tx_bits_wanted = n_tx;

	if (status == 0 && self->connected == 0) {
		/*
		 * The edge into carrier.  Report the line rates and let the
		 * next block start fetching data.
		 *
		 * BOTH DIRECTIONS ARE TOLD THE SAME RATE, which for an
		 * asymmetric modem cannot be right in both -- see D24.  The
		 * `self->dp.modem` here rather than `dp->modem` is the
		 * original's too; the two are the same pointer.
		 */
		int rate = (self->caller == 0) ? V23_RATE_BACKWARD
					       : V23_RATE_FORWARD;

		self->connected = 1;
		modem_set_param(self->dp.modem, MDMPRM_TX_RATE, rate);
		modem_set_param(self->dp.modem, MDMPRM_RX_RATE, rate);
		status = DPSTAT_CONNECT;
	} else if (status == 0) {
		status = DPSTAT_OK;
	} else if (status == 1) {
		self->connected = 0;
		status = DPSTAT_OK;
	} else {
		self->connected = 0;
		status = DPSTAT_ERROR;
	}

	/* The original prints "v23: V23STAT: --> %d\n" on a change. */
	dp->status = (unsigned)status;
	return status;
}

/*
 * ---------------------------------------------------------------------------
 * Layout, pinned.  32-bit ABI only.
 */
#if defined(__SIZEOF_POINTER__) && __SIZEOF_POINTER__ == 4

#define V23DP_ASSERT_OFF(field, off) \
	typedef char v23dp_off_##field[ \
		((int)__builtin_offsetof(struct v23_dp, field) == (off)) \
		? 1 : -1]

V23DP_ASSERT_OFF(caller, 0x014);
V23DP_ASSERT_OFF(tx_bits_wanted, 0x018);
V23DP_ASSERT_OFF(connected, 0x01c);
V23DP_ASSERT_OFF(modem, 0x020);
V23DP_ASSERT_OFF(wrapper, 0x024);
V23DP_ASSERT_OFF(tx_bits, 0x028);
V23DP_ASSERT_OFF(rx_bits, 0x1b8);

/* 0x348 is the size sysdep_malloc is asked for. */
typedef char v23dp_size[(sizeof(struct v23_dp) == 0x348) ? 1 : -1];

#endif

/*
 * b103.c -- Bell 103 / V.21: datapump registration and glue.
 *
 * Reconstructed from dsplibs.o b103.c:
 *   dp_b103_init   .text 0x005840
 *   dp_b103_exit   .text 0x005880
 *   b103_ops       .data  0x0090
 *   b103_create    .text 0x005400   (pending -- see below)
 *   b103_delete    .text 0x0055a0   (pending)
 *   b103_process   .text 0x0055f0   (pending)
 *
 * This is the thin layer between the modem core and the Bell 103 modulation.
 * It owns nothing of the DSP; it registers the datapump, allocates the state,
 * and wires the modulation into dp_wrapper.
 *
 * The shape is worth reading off the ops table, because it explains the whole
 * datapump architecture in one line: `process` is **dp_wrapper_run**, not
 * b103_process.  The modem core always calls the wrapper, and the wrapper
 * calls b103_process at the datapump's own rate and fragment size once it has
 * buffered and rate-converted.  b103_process is handed to
 * dp_wrapper_create() as a function pointer and is never reachable from the
 * ops table at all.
 *
 * STATUS: registration is complete and verified.  b103_create, b103_delete
 * and b103_process are not yet reconstructed -- they depend on B103FP_create
 * (0x867 bytes), B103FP_delete and B103FP_modem, which are the modulation
 * proper.  Until those land, the ops table carries NULL for create and
 * delete, so the module registers correctly but cannot yet build a datapump.
 */

#include <stddef.h>

#include "dsplib/b103.h"
#include "dsplib/dp_wrapper.h"
#include "dsplib/sysdep.h"
#include "dsplib/modem_params.h"

/*
 * `modem_dp_register`, `modem_dp_deregister`, `modem_get_bits`,
 * `modem_put_bits` and `modem_get_sreg` are DECLARED BY THE VENDORED HEADERS
 * now (`third_party/slmodem/modem_dp.h` and `modem_defs.h`, reached through
 * `dsplib/dp.h`).  This file used to declare the ones it needed locally, and
 * three of those declarations disagreed with the author's own -- see F8412.
 */

/*
 * Bell 103 and V.21 share one implementation: same 300 bit/s FSK, differing
 * only in tone frequencies, which b103_create selects from the DP_ID.  That
 * is why one ops table is registered under both.
 */
/*
 * FILE-LOCAL, read off the relocation form.  `dp_b103_init` and
 * `dp_b103_exit` reach this table in the blob as `.data` plus an inline
 * addend, not as a named symbol, and by lever 5's rule that means the
 * original's was `static` -- the same evidence that settled
 * `RcFixed_Check_Combination`.  `src/v8/v8dp.c`'s `v8_op` is the tree's own
 * precedent and has always been spelt this way.
 *
 * F7768 measured the change and could not take it: the tests named this
 * symbol directly, so making it static needed an accessor.  It does not --
 * the harness already records what the module REGISTERS, on both sides, and
 * that is the path the modem core itself takes.  Finding F8121.
 */
static struct dp_operations b103_ops = {
	.name = "b103",
	.create = b103_create,
	.delete = b103_delete,
	/*
	 * `process` is dp_wrapper_run, NOT b103_process.  The core always
	 * calls the wrapper; the wrapper calls b103_process at the datapump's
	 * own rate and fragment size once it has buffered and rate-converted.
	 */
	.process = (int (*)(struct dp *, void *, void *, int))dp_wrapper_run
	/* use_count and hangup are zero */
};

int
dp_b103_init(void)
{
	modem_dp_register(DP_B103, &b103_ops);
	modem_dp_register(DP_V21, &b103_ops);
	return 0;
}

void
dp_b103_exit(void)
{
	modem_dp_deregister(DP_B103, &b103_ops);
	modem_dp_deregister(DP_V21, &b103_ops);
}

/*
 * ---------------------------------------------------------------------------
 * b103_create -- .text 0x005400, 406 bytes.
 *
 * The glue: allocate the datapump's own state, wire a dp_wrapper in front of
 * b103_process, and build the B103FP configuration from the two arguments
 * that actually carry information.
 *
 * THE CONFIGURATION IS BUILT HERE, not taken from B103_CFG.  That is what
 * makes a link possible -- `B103_CFG` is the loopback template and would
 * never complete a call (docs/configuration.md).  Only two of its seven words
 * depend on the caller:
 *
 *     call_type = (caller == 0)     0 originate, 1 answer
 *     v21       = (id == DP_V21)
 *
 * Note the polarity of the first: `caller` non-zero means this station placed
 * the call, which is `B103_CALL_ORIGINATE`, which is **zero**.  The two
 * senses are opposite and the `sete` is what flips them.
 *
 * The remaining five are constants, and two of them differ from the built-in
 * template in ways that matter: the answer-tone timeout is 60000 ticks rather
 * than 14000 -- 3000 blocks rather than 700 -- and the transmit scale is 6200
 * rather than 3200.
 */
struct dp *
b103_create(struct modem *modem, enum DP_ID id, int caller, int srate, int max_frag,
	    struct dp_operations *op)
{
	struct b103_cfg cfg;
	struct b103_dp *dp;

	(void)max_frag;

	dp = (struct b103_dp *)sysdep_malloc(sizeof(*dp));
	if (dp == NULL)
		return NULL;
	sysdep_memset(dp, 0, sizeof(*dp));

	dp->caller = caller;
	dp->dp.id = id;
	dp->dp.modem = modem;
	dp->dp.op = op;

	dp->wrapper = dp_wrapper_create(dp, b103_process, B103_DP_FRAG, srate,
					B103_DP_SRATE);
	if (dp->wrapper == NULL) {
		sysdep_free(dp);
		return NULL;
	}
	dp->dp.dp_data = dp->wrapper;
	dp->wrapper->dp = &dp->dp;

	cfg.call_type = (caller == 0) ? B103_CALL_ANSWER : B103_CALL_ORIGINATE;
	cfg.v21 = (id == DP_V21);
	cfg.loop_high_channel = 0;
	cfg.tone_timeout_ticks = 60000;		/* 3000 blocks */
	cfg.f10 = 1;
	cfg.f14 = 700;
	cfg.tx_scale = 6200;

	dp->last_status = 1;
	dp->fp = B103FP_create(NULL, &cfg);
	if (dp->fp == NULL) {
		dp_wrapper_delete(dp->wrapper);
		sysdep_free(dp);
		return NULL;
	}

	return &dp->dp;
}

/*
 * b103_delete -- .text 0x0055a0, 72 bytes.
 *
 * Reaches its own state the long way round -- out through `dp_data` to the
 * wrapper and back through the wrapper's back-pointer -- which lands exactly
 * where it started, since `struct dp` is the head of `struct b103_dp`.
 * Reproduced as written: it is how the original documents the relationship,
 * and it is what would still work if the two were ever separated.
 */
int
b103_delete(struct dp *dp)
{
	struct b103_dp *self = (struct b103_dp *)
		((struct dp_wrapper *)dp->dp_data)->dp;

	B103FP_delete(self->fp);
	dp_wrapper_delete(self->wrapper);
	dp->dp_data = NULL;
	sysdep_free(self);
	return 0;
}

/*
 * ---------------------------------------------------------------------------
 * b103_process -- .text 0x0055f0, 582 bytes.
 *
 * One block, called by dp_wrapper at the datapump's own rate: fetch bits from
 * the modem core, run B103FP_modem, hand recovered bits back, and translate
 * B103's internal status into the DPSTAT_* the core understands.
 *
 * ---------------------------------------------------------------------------
 * One buffer, two widths
 *
 * `modem_get_bits` and `modem_put_bits` deal in one BYTE per bit; B103FP_modem
 * deals in one INT per bit.  Rather than keep two buffers, the same one is
 * widened in place -- and walked BACKWARDS, which is what makes that safe:
 * writing element i as an int cannot clobber element i+1 as a byte if you
 * start from the end.
 *
 * ---------------------------------------------------------------------------
 * The status translation
 *
 * B103FP_modem returns the word at fp+0x1c; its low byte is B103's own status
 * (findings F34 and F36).  This is where it becomes a DPSTAT_*:
 *
 *     B103 0      -> DPSTAT_OK,      and start asking for data
 *     B103 1..4   -> DPSTAT_OK,      still setting up
 *     B103 5, 6   -> DPSTAT_ERROR,   the call failed
 *     B103 7      -> DPSTAT_CONNECT
 *
 * The transition INTO connect -- and only the transition -- reports the line
 * rate to the core, 300 bit/s each way, and turns on the data path by setting
 * `tx_bits_wanted`.  Reporting it every block would be harmless; reporting it
 * on the transition is what makes `dp->status` the edge trigger the core
 * expects.
 */
int
b103_process(void *dp_arg, void *in, void *out, int count)
{
	struct dp *dp = (struct dp *)dp_arg;
	struct b103_dp *self = (struct b103_dp *)
		((struct dp_wrapper *)dp->dp_data)->dp;
	short n_tx;
	short n_rx;
	int status;
	int result;
	int i;

	(void)count;

	if (self->tx_bits_wanted != 0) {
		/*
		 * Fetch as bytes into the same buffer B103FP_modem will read
		 * as ints, then widen backwards.
		 */
		n_tx = (short)self->tx_bits_wanted;
		n_tx = (short)modem_get_bits(dp->modem, 1,
					     (unsigned char *)self->tx_bits,
					     (unsigned short)n_tx);
		for (i = (unsigned short)n_tx; i >= 0; i--)
			self->tx_bits[i] =
				((unsigned char *)self->tx_bits)[i];
	} else {
		n_tx = B103_BITS_PER_BLOCK;
	}

	n_rx = B103_DP_FRAG;
	result = B103FP_modem(self->fp, self->tx_bits, (short *)out,
			      (short *)in, self->rx_bits, &n_tx, &n_rx);

	/* Hand recovered bits back, narrowing ints to bytes in place. */
	if (self->tx_bits_wanted != 0 && n_rx != 0) {
		for (i = 0; i < (unsigned short)n_rx; i++)
			((unsigned char *)self->rx_bits)[i] =
				(unsigned char)(self->rx_bits[i] & 1);
		modem_put_bits(dp->modem, 1, (unsigned char *)self->rx_bits,
			       (unsigned short)n_rx);
	}

	if (result != self->last_status)
		self->last_status = result;

	status = result & 0xff;
	if (status == 0) {
		self->tx_bits_wanted = B103_BITS_PER_BLOCK;
		result = DPSTAT_OK;
	} else if (status == 5 || status == 6) {
		self->tx_bits_wanted = 0;
		result = DPSTAT_ERROR;
	} else if (status == 7) {
		result = DPSTAT_CONNECT;
	} else {
		self->tx_bits_wanted = 0;
		result = DPSTAT_OK;
	}

	/*
	 * Only on the edge into connect: tell the core the line rate and
	 * open the data path.
	 */
	if ((unsigned)result != dp->status && result == DPSTAT_CONNECT) {
		self->tx_bits_wanted = B103_BITS_PER_BLOCK;
		modem_set_param(dp->modem, MDMPRM_TX_RATE, B103_LINE_RATE);
		modem_set_param(dp->modem, MDMPRM_RX_RATE, B103_LINE_RATE);
	}

	dp->status = (unsigned)result;
	return result;
}

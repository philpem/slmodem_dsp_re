/*
 * v32.c -- ITU-T V.32 / V.32bis: datapump registration and glue.
 *
 * Reconstructed from dsplibs.o v32.c:
 *   v32_create   .text 0x004560   627 bytes   (file-local)
 *   v32_delete   .text 0x0047e0    95 bytes   (file-local)
 *   v32_process  .text 0x004840   871 bytes   (file-local)
 *   dp_v32_init  .text 0x004bb0    49 bytes
 *   dp_v32_exit  .text 0x004bf0    49 bytes
 *   v32_ops      .data  0x000048    24 bytes  (file-local)
 *
 * The thin layer between the modem core and the V.32 modulation, and it is
 * `v23.c` with V.32's numbers -- same `struct dp` header, same ops table,
 * same `dp_wrapper` in front of an 8 kHz fragment, the same trick of widening
 * a byte buffer into an int buffer in place, and the same 840-byte object
 * with the two bit buffers at +0x28 and +0x1b8.  Reading the two side by side
 * is the fastest way to see what belongs to V.32 and what belongs to the
 * datapump architecture.
 *
 * ---------------------------------------------------------------------------
 * TWO IDS, ONE TABLE
 *
 * `dp_v32_init` calls `modem_dp_register` TWICE with the same `&v32_ops`, for
 * 32 and 132, and `dp_v32_exit` deregisters both in the same order.  The
 * numbers are the vendored header's: `ref/slmodemd/modem_defs.h` has
 * `DP_V32 = 32` and `DP_V32BIS = 132` in `enum DP_ID`.  `b103.c` does the
 * same thing for `DP_V21` and `DP_B103`, so this is the architecture's idiom
 * and not V.32's own.
 *
 * `dp_v32_init` RETURNS THE SECOND REGISTRATION'S RESULT rather than a
 * literal zero: there is no `xor %eax,%eax` before its `ret`, where
 * `dp_b103_init` and `prop_dp_init` both have one.
 *
 * ---------------------------------------------------------------------------
 * `modem_get_bits`'s SECOND ARGUMENT IS A WIDTH, AND THE VENDORED HEADER SAYS SO
 *
 * `ref/slmodemd/modem_dp.h` declares
 *
 *     extern int modem_get_bits(struct modem *m, int nbits, u8 *buf, int n);
 *
 * -- `nbits`, not a channel number.  V.32 passes `line_rate / 2400`, which is
 * 6 at 14400 and 3 at 7200, and the receive side masks each returned element
 * to exactly that many bits before handing it back.  V.23 and Bell 103 pass 1
 * because they are one bit per element, which is why `v23.c`'s local
 * declaration calls the parameter `chan` and gets away with it.
 *
 * The count is an `int` here and nothing zero-extends it, which is the other
 * half of the same correction: `v23.c` declares it `unsigned short`.
 *
 * ---------------------------------------------------------------------------
 * THE BLOCK IS 40 SAMPLES, WHICH IS TWELVE SYMBOLS
 *
 * `dp_wrapper_create` is called with a fragment of 40 and a datapump rate of
 * 8000, so one `v32_process` is 5 ms; V.32 is 2400 baud, so that is exactly
 * twelve symbols, and twelve is what `v32_process` puts in
 * `symbols_per_block` when the line comes up.  It is zero until then, and
 * that -- rather than V.23's separate `connected` flag -- is what keeps the
 * core's data off a line that is still training: `modem_get_bits` is called
 * unconditionally, for zero symbols.
 *
 * ---------------------------------------------------------------------------
 * THE RECEIVE COUNT IS CLAMPED TO A HUNDRED, LOUDLY
 *
 * `rx_bits` is a hundred elements and `v32_process` says so twice: it prints
 * `v32: FATAL: rx_len is too big (%d).` and then clamps, rather than trusting
 * the datapump.  The `.bss` scratch pair `V32FP_modem` copies through --
 * `tx_in_internal` and `rx_out_internal`, 0xc8 bytes each -- is a hundred
 * shorts as well, so the bound is the same one at both ends.
 *
 * THE CLAMP IS ONLY REACHED WHEN `symbols_per_block` IS NON-ZERO.  When it is
 * zero the count is forced to zero instead and the test never runs, so a
 * datapump that reported a huge count before connect would be silently
 * discarded rather than clamped.  That is the object's ordering and is
 * reproduced.
 */

#include "dsplib/debug.h"
#include "dsplib/dp.h"
#include "dsplib/dp_wrapper.h"
#include "dsplib/modem_params.h"
#include "dsplib/sysdep.h"
#include "dsplib/v32.h"
#include "dsplib/v32fpctl.h"
#include "dsplib/v32fpstat.h"

/* Provided by the modem core (slmodemd/modem.c and modem_dp.c). */

extern int modem_dp_register(int id, void *op);
extern void modem_dp_deregister(int id, void *op);
extern int modem_get_bits(void *modem, int nbits, unsigned char *buf, int n);
extern int modem_put_bits(void *modem, int nbits, unsigned char *buf, int n);

/*
 * FILE-LOCAL, on the same evidence as `b103.c`'s `b103_ops` and `v23.c`'s
 * `v23_ops`: `nm` gives it as `d`, and `dp_v32_init` is the only referrer.
 * Finding F8121.
 */
static struct dp_operations v32_ops = {
	"v32",			/* .rodata.str1.1 + 0x526                    */
	0,			/* use_count                                 */
	v32_create,
	v32_delete,
	/*
	 * `process` is `dp_wrapper_run`, NOT `v32_process` -- see `b103.c` for
	 * what that says about the architecture.  `v32_process` reaches the
	 * table only by having been handed to `dp_wrapper_create`.
	 */
	(int (*)(struct dp *, void *, void *, int))dp_wrapper_run,
	0			/* hangup                                    */
};

/*
 * ---------------------------------------------------------------------------
 * v32_create -- .text 0x004560.
 *
 * THE PHYSICAL DELAY IS THE ONLY THING HERE THAT IS NOT A CONSTANT, and it is
 * the one number the echo canceller needs from the host.
 *
 *     delay = modem_get_param(MDMPRM_IODELAY) + 48
 *
 * and if that exceeds 216 the excess is handed BACK to the core as a negative
 * `MDMPRM_UPDATE_DELAY` before the value is clamped -- so the core is told by
 * how much its own figure was refused.  The 48 and the 216 are the object's.
 *
 * What reaches the datapump is `delay * 5 / 6`, a signed divide.  The 5/6 is
 * not the 9/10 of `MRFv32_CFG`'s resampler and is not explained by anything
 * reconstructed; it is recorded as the object's arithmetic.
 *
 * The rate is `MDMPRM_MAX_RATE` clamped to 14400 by an UNSIGNED comparison,
 * so a negative maximum reads as enormous and comes back as 14400.
 */
struct dp *
v32_create(void *modem, int id, int caller, int srate, int max_frag,
	   struct dp_operations *op)
{
	struct v32fp_cfg cfg;
	struct v32_dp *self;
	struct dp_wrapper *w;
	int delay;
	int rate;

	(void)max_frag;

	if (DSPLIB_DEBUG_ON())
		dsplibs_debug_printf("v32: create...\n");

	self = sysdep_malloc(sizeof(*self));
	if (self == 0)
		return 0;
	sysdep_memset(self, 0, sizeof(*self));

	self->bits_per_symbol = 6;		/* 14400 / 2400              */
	self->line_rate = 14400;
	self->symbols_per_block = 0;
	self->dp.id = id;
	self->dp.op = op;
	self->dp.modem = modem;

	w = dp_wrapper_create(self, v32_process, V32_DP_FRAG, srate,
			      V32_DP_SRATE);
	self->wrapper = w;
	if (w == 0) {
		sysdep_free(self);
		return 0;
	}
	w->dp = &self->dp;
	self->dp.dp_data = w;

	cfg.protocol = (caller == 0);
	cfg.timeout = 60000;
	cfg.energy_drop_time = 700;
	cfg.r10 = 1;

	rate = modem_get_param(modem, MDMPRM_MAX_RATE);
	if ((unsigned int)rate > 14400)
		rate = 14400;
	cfg.r16 = (unsigned short)rate;
	cfg.rate = (unsigned short)rate;

	delay = modem_get_param(modem, MDMPRM_IODELAY) + 48;
	if (delay > 216) {
		modem_set_param(modem, MDMPRM_UPDATE_DELAY, 216 - delay);
		delay = 216;
	}
	if (DSPLIB_DEBUG_ON())
		dsplibs_debug_printf("v32: phys. delay is %d\n", delay);
	cfg.phys_delay = delay * 5 / 6;

	if (DSPLIB_DEBUG_ON())
		dsplibs_debug_printf("v32: V32 config S%d,R%d,T%d,A%d,L%d\n",
				     cfg.protocol, (int)cfg.rate, cfg.timeout,
				     cfg.r10, cfg.energy_drop_time);

	self->fp = V32FP_create(&cfg, 0);
	if (self->fp == 0) {
		/*
		 * The second test can never fire -- `self->fp` was just found
		 * null -- and the object makes it anyway, so it is here.  The
		 * wrapper-creation failure above skips both tests, which is
		 * also the object's shape.
		 */
		if (self->wrapper != 0)
			dp_wrapper_delete(self->wrapper);
		if (self->fp != 0)
			V32FP_delete(self->fp);
		sysdep_free(self);
		return 0;
	}

	return &self->dp;
}

/*
 * v32_delete -- .text 0x0047e0.
 *
 * Reaches its own state out through `dp_data` and back through the wrapper's
 * back-pointer, landing exactly where it started.  Same as `v23_delete` and
 * `b103_delete`, and reproduced for the same reason.
 */
int
v32_delete(struct dp *dp)
{
	struct v32_dp *self = (struct v32_dp *)
		((struct dp_wrapper *)dp->dp_data)->dp;

	if (DSPLIB_DEBUG_ON())
		dsplibs_debug_printf("v32: delete...\n");

	V32FP_delete(self->fp);
	dp_wrapper_delete(self->wrapper);
	dp->dp_data = 0;
	sysdep_free(self);
	return 0;
}

/*
 * ---------------------------------------------------------------------------
 * v32_process -- .text 0x004840.
 *
 * One block, called by `dp_wrapper` at the datapump's own rate.
 *
 * ---------------------------------------------------------------------------
 * THE STATUS TRANSLATION IS A 29-ENTRY JUMP TABLE, AND IT IS THE SAME CODE
 * SPACE `V32_CONNECT` HOLDS
 *
 * `V32FP_modem` returns `V32_OBJ_STATUS`, and `.rodata + 0x1a8` maps 0..28
 * onto four arms:
 *
 *     3, 4, 13, 24, 25, 26, 27          -> DPSTAT_CONNECT
 *     12, 14                            -> "nocarrier", DPSTAT_ERROR
 *     16 .. 23                          -> "error",     DPSTAT_ERROR
 *     0, 1, 2, 5..11, 15, 28            -> DPSTAT_OK
 *     anything above 28                 -> "unknown",   DPSTAT_ERROR
 *
 * `v32hdx_tables.c` reads `V32_CONNECT` as "per-RATE status codes, and the
 * table's own name is the only evidence about what they mean".  Its seven
 * entries are {4, 3, 25, 24, 26, 27, 14}, and **six of the seven land in the
 * CONNECT arm above while the seventh, the `V32_RATE_NONE` slot, lands in the
 * no-carrier arm.**  That is independent corroboration of F8585 from the
 * consumer's side, and it is why the arms are not renamed here: the codes are
 * the author's and this table is what they mean to the host.  Finding F8646.
 */
int
v32_process(void *dp_arg, void *in, void *out, int count)
{
	struct dp *dp = (struct dp *)dp_arg;
	struct v32_dp *self = (struct v32_dp *)
		((struct dp_wrapper *)dp->dp_data)->dp;
	struct v32_status st;
	short *cleaned;
	int ncleaned;
	int ntx;
	int nrx;
	int status;
	int i;

	if (dp->status == DPSTAT_ERROR) {
		sysdep_memset(out, 0, count * (int)sizeof(short));
		return DPSTAT_ERROR;
	}

	ntx = self->symbols_per_block;
	nrx = count;
	ntx = modem_get_bits(dp->modem, self->bits_per_symbol,
			     (unsigned char *)self->tx_bits, ntx);
	/*
	 * Widen BACKWARDS -- writing element i as an int cannot clobber
	 * element i+1 as a byte if you start from the end.  `v23_process`
	 * carries the same loop.
	 */
	for (i = ntx - 1; i >= 0; i--)
		self->tx_bits[i] = ((unsigned char *)self->tx_bits)[i];

	nrx = count;
	status = V32FP_modem(self->fp, self->tx_bits, (short *)out,
			     (const short *)in, self->rx_bits, &ntx, &nrx);

	ncleaned = 0;
	cleaned = V32FP_GetCleanedSamples(self->fp, &ncleaned);
	if (cleaned != 0 && ncleaned > 0)
		modem_debug_log_data(dp->modem, 3, cleaned,
				     ncleaned * (int)sizeof(short));

	/*
	 * `jbe` and not `jle`: the bound test is UNSIGNED, so a negative count
	 * reads as enormous and is clamped to a hundred rather than skipped.
	 */
	if (self->symbols_per_block == 0) {
		nrx = 0;
	} else if ((unsigned int)nrx > V32_BIT_BUFFER - 1) {
		if (DSPLIB_DEBUG_ON())
			dsplibs_debug_printf(
				"v32: FATAL: rx_len is too big (%d).\n", nrx);
		nrx = V32_BIT_BUFFER;
	}

	for (i = 0; i < nrx; i++)
		((unsigned char *)self->rx_bits)[i] =
			(unsigned char)((1 << self->bits_per_symbol) - 1)
			& (unsigned char)self->rx_bits[i];
	modem_put_bits(dp->modem, self->bits_per_symbol,
		       (unsigned char *)self->rx_bits, nrx);

	switch ((unsigned char)status) {
	case 0: case 1: case 2: case 5: case 6: case 7: case 8: case 9:
	case 10: case 11: case 15: case 28:
		status = DPSTAT_OK;
		break;

	case 12: case 14:
		if (DSPLIB_DEBUG_ON())
			dsplibs_debug_printf("v32: process: nocarrier.\n");
		status = DPSTAT_ERROR;
		break;

	case 16: case 17: case 18: case 19:
	case 20: case 21: case 22: case 23:
		if (DSPLIB_DEBUG_ON())
			dsplibs_debug_printf("v32: process: error.\n");
		status = DPSTAT_ERROR;
		break;

	case 3: case 4: case 13: case 24: case 25: case 26: case 27:
		if (DSPLIB_DEBUG_ON())
			dsplibs_debug_printf("v32: process: connect %x.\n",
					     (unsigned char)status);
		V32FP_status(self->fp, &st);
		switch (st.tx_rate) {
		case 4800:
			self->line_rate = 4800;
			break;
		case 7200:
			self->line_rate = 7200;
			break;
		case 9600:
			self->line_rate = 9600;
			break;
		case 12000:
			self->line_rate = 12000;
			break;
		case 14400:
			self->line_rate = 14400;
			break;
		default:
			break;
		}
		self->symbols_per_block = 12;
		self->bits_per_symbol = (int)(self->line_rate / V32_BAUD);
		if (DSPLIB_DEBUG_ON())
			dsplibs_debug_printf("v32: v32_update connect: " "tx_rate %d, rx_rate %d\n",
					     st.tx_rate, st.rx_rate);
		/*
		 * `self->dp.modem` here rather than `dp->modem`, which is the
		 * original's and is the same pointer -- `v23_process` carries
		 * the same note.
		 */
		modem_set_param(self->dp.modem, MDMPRM_TX_RATE, st.tx_rate);
		modem_set_param(self->dp.modem, MDMPRM_RX_RATE, st.rx_rate);
		status = DPSTAT_CONNECT;
		break;

	default:
		if (DSPLIB_DEBUG_ON())
			dsplibs_debug_printf(
				"v32: process: unknown V.32 status: %d.\n",
				(unsigned char)status);
		status = DPSTAT_ERROR;
		break;
	}

	dp->status = (unsigned)status;
	return status;
}

/* ------------------------------------------------------------------------ */

int
dp_v32_init(void)
{
	modem_dp_register(DP_V32, &v32_ops);
	return modem_dp_register(DP_V32BIS, &v32_ops);
}

void
dp_v32_exit(void)
{
	modem_dp_deregister(DP_V32, &v32_ops);
	modem_dp_deregister(DP_V32BIS, &v32_ops);
}

/*
 * ---------------------------------------------------------------------------
 * Layout, pinned.  32-bit ABI only.
 */
#if defined(__SIZEOF_POINTER__) && __SIZEOF_POINTER__ == 4

#define V32DP_ASSERT_OFF(field, off) \
	typedef char v32dp_off_##field[ \
		((int)__builtin_offsetof(struct v32_dp, field) == (off)) \
		? 1 : -1]

V32DP_ASSERT_OFF(bits_per_symbol, 0x014);
V32DP_ASSERT_OFF(line_rate, 0x018);
V32DP_ASSERT_OFF(symbols_per_block, 0x01c);
V32DP_ASSERT_OFF(fp, 0x020);
V32DP_ASSERT_OFF(wrapper, 0x024);
V32DP_ASSERT_OFF(tx_bits, 0x028);
V32DP_ASSERT_OFF(rx_bits, 0x1b8);

/* 0x348 is the size sysdep_malloc is asked for -- the same as V.23's. */
typedef char v32dp_size[(sizeof(struct v32_dp) == 0x348) ? 1 : -1];

#endif

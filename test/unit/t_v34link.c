/*
 * t_v34link.c -- a whole call: V.8, the datapump change, V.34, and DATA.
 *
 * THIS IS THE FIRST V.34 CONNECTION IN THE TREE.  Two endpoints, one
 * originating and one answering, over a delayed wire: they negotiate V.8,
 * hand over the way `slmodemd/modem.c`'s `do_modem_change_dp` does, run the
 * V.34 startup to completion, agree 33,600 bit/s each way, enter data mode
 * and carry bits BOTH WAYS AT BER 0.
 *
 * WHAT IT IS NOT.  Everything driven here is THE BLOB'S OWN CODE on both
 * ends.  `vpcm_run` -- .text 0x3e40, the `.process` of the VPCM operations
 * table -- is the only function in the object that calls `modem_get_bits` and
 * `modem_put_bits` for V.34, and it is not reconstructed, so no mixed run is
 * possible through it.  What this file establishes is the ORACLE: a
 * configuration in which the original connects and carries data, which is
 * what `t_v34conn.c`'s four-way comparison has never had.  Finding F963.
 *
 * WHY IT CONNECTS AND `t_v34conn` DOES NOT, in one word: `MDMPRM_IODELAY`.
 * `CFG_IODELAY` below is derived from `slmodemd`'s own drivers rather than
 * chosen, and the mechanism it feeds is written out at that constant.  It is
 * NOT a knob turned until something happened: the guard, the formula and the
 * threshold were read out of the code first and the boundary is sharp and
 * predicted.  Findings F960-962.
 *
 * THE LINK FROM V.8 TO V.34 IS ONE BLOCK OF MEMORY.  `v8_create` takes
 * `MDMPRM_DPRUNTIME` as its call menu -- the `struct v8_cm` the handshake
 * reads and `V8UpdateModemParameters` writes the agreed result back into --
 * and `vpcm_create` at .text 0x3ac3 calls the same `dp_param_get` and keeps
 * the pointer at root+0x28.  It writes +0x30, +0x34, +0x38, +0x3c, +0x64,
 * +0x68, +0x6c and +0x78 of that block and touches only bits 4 and 5 of `b2`;
 * it never clears `b0`, `b1`, `offered` or `menu`.  So the negotiated menu
 * survives into V.34 -- and finding F961 records the measurement that says the
 * V.34 handshake does not read it: with the block zeroed instead, the
 * trajectory is identical, block for block.  V.8 running is proved here and
 * is NOT what makes the call connect.
 */

#include <signal.h>
#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <unistd.h>

#include "harness.h"

#include "dsplib/dp.h"
#include "dsplib/modem_params.h"
#include "dsplib/v8dp.h"

extern void ref_dp_v8_init(void);
extern void ref_dp_vpcm_init(void);
extern unsigned int ref_dsplibs_debug_level;
extern unsigned int dsplibs_debug_level;

/* --- the V.34 object, by offset (the same ones `t_v34conn.c` uses) -------- */

#define ROOT_V34	0x2c

#define O_MODE		0x2218	/* int:   2 handshaking, 1 CONNECTED */
#define O_MICROSTATE	0x3592
#define O_RXSTATE	0x3594
#define O_TXSTATE	0x3596
#define O_ROLE		0x359c	/* short: 0x65 originate, 0x66 answer */
/*
 * The two connection rates in units of 2400 bit/s: `v34handshak` at 0x63d1a
 * and 0x63d32 multiplies each by 0x960 to print them, and writes mode 1 five
 * instructions later.  Zero while the call has not converged.
 */
#define O_TXRATE	0xaa88
#define O_RXRATE	0xaa98

/*
 * The three words arm 47 `TX_PHASE2_ANS` reads, so a run that aborts there
 * can say WHICH guard fired rather than only that one did.  `T3M_COUNTER`,
 * `T3M_FILTDELAY` and `T3M_F3588` in `src/pump/v34/v34hshak.c`.
 */
#define O_COUNTER	0xaa78
#define O_FILTDELAY	0xaa7c
#define O_F3588		0x3588

#define ROLE_ORIGINATE	0x65
#define ROLE_ANSWER	0x66

#define MODE_HANDSHAKE	2
#define MODE_CONNECTED	1

/* 14 * 2400 = 33,600, which is what the transcript prints. */
#define RATE_33600	14
/* `V34HSHAKE: txstate EXMIT=>DATAXMIT` -- the last transition of the call. */
#define TXSTATE_DATAXMIT	70

/* --- the call ------------------------------------------------------------ */

#define NEP		2
#define SRATE		9600	/* `cmp $0x2580,%esi` -- vpcm_create's only rate */
#define FRAG		48	/* `cmpl $0x30,...` / `jg` -- and its cap       */

/*
 * THE WIRE IS 30 ms EACH WAY (finding F903): `t_v34call`'s one-sample wire
 * draws "RTD (1) lower than min (30), masking Far EC..." out of the object
 * itself, and 288 samples at 9,600 Hz is 30 ms.
 */
#define WIRE_DELAY	288
#define WIRE_MAX	8192	/* the diagnostic ceiling; the delay is a variable */

#define CFG_MIN_RATE	2400
#define CFG_MAX_RATE	33600
#define CFG_BUFWORDS	512

/*
 * `MDMPRM_IODELAY`, AND IT IS RECOVERED FROM THE HOST RATHER THAN CHOSEN.
 *
 * `vpcm_create` reads parameter 5 at .text 0x3bed and does
 *
 *     edx = iodelay + 4;  if (0xf4 - edx < 0) fail;
 *     rt[0x64] = edx;  rt[0x68] = edx - 0x30 + root[0xd254];
 *
 * so the object's own acceptance window is `iodelay <= 240`.  What a host
 * actually puts there, from `slmodemd/modem_main.c`:
 *
 *   modemap  `modemap_start` writes 192*2 bytes and sets `dev->delay = ret/2`
 *            = 192; `modemap_ioctl` returns the kernel's own answer -- which
 *            its own comment gives as `s->delay + ST7554_HW_IODELAY (48)`
 *            bytes, 24 samples -- PLUS that 192.  About 216.
 *   alsa     `alsa_start` writes `period * buf_periods` frames with the short
 *            buffer, 48 * SHORT_BUFFER_PERIODS(4) = 192, and adds
 *            INTERNAL_DELAY (40): 232.  Its LONG-buffer path writes 384 and
 *            reaches 424, which is ABOVE the object's 240 cap and would be
 *            refused outright -- recorded because it is a real asymmetry and
 *            not something to smooth over.
 *   socket   returns 0.  A stub; its own comment says what the kernel module
 *            would have returned.
 *
 * Two independent measuring drivers land at 216 and 232 and the third is a
 * stub.  216 is used because it is the one derived from a driver that asks
 * the hardware.  Every value from 88 up to the object's own cap of 240
 * connects identically at 33,600 (finding F962), so this does not sit on a
 * point: it sits in the middle of a half-open interval whose lower end the
 * mechanism predicts.
 */
#define CFG_IODELAY	216

/* Long enough for V.8's own 12-second deadline (`cfg.timeout_a = 0x0c`). */
#define V8_MAX_BLOCKS	3000	/* 144,000 samples, 15 s */

/*
 * The V.34 startup takes 1,591 blocks; everything after that is data.  4,000
 * blocks is 192,000 samples and 20 s of line time.
 */
#define V34_BLOCKS	4000

static const char *const ep_name[NEP] = { "originate", "answer" };
static const int ep_caller[NEP] = { 1, 0 };
static const short ep_role[NEP] = { ROLE_ORIGINATE, ROLE_ANSWER };

/*
 * TWO DISTINCT MODEM HANDLES, and they are load-bearing rather than tidy.
 * Both endpoints are the blob's, so both land on the reference side of the
 * bit-pipe shim; without routing they would draw from one cursor and write
 * into one sink, and a BER measured off that reads zero whether or not a
 * single bit crossed the wire.  `harness_modem_route_add` gives each handle
 * its own pair of shims and its own pattern.
 */
static void *const ep_modem[NEP] = { (void *)0xD1A1u, (void *)0xD1A2u };

/*
 * ONE PATTERN PER ENDPOINT, AND THEY DIFFER.  Two endpoints sending the same
 * bits cannot tell a working link from a shim that handed each of them the
 * other's stream -- so the claim below is that an endpoint received the OTHER
 * one's pattern, which is false in both of those failure modes.  31 is
 * coprime with the 16-bit words V.34 frames data into, so the pattern does
 * not align with the framing.
 */
#define PAT_LEN	31
static unsigned char pattern[NEP][PAT_LEN];

static void
make_patterns(void)
{
	unsigned s = 1u;
	int ep, i;

	for (ep = 0; ep < NEP; ep++) {
		s = 1u + (unsigned)ep * 7919u;
		for (i = 0; i < PAT_LEN; i++) {
			s = s * 1103515245u + 12345u;
			pattern[ep][i] = (unsigned char)((s >> 19) & 1);
		}
	}
}

/*
 * THE RUNTIME BLOCK, one per endpoint.  V.8 gets it as its call menu and V.34
 * gets the same one.  Bigger than `struct v8_cm` because `vpcm_create` writes
 * as far as +0x78 through it; `dspinfo` is separate, and is where V.8
 * publishes the two words it agreed.
 *
 * The parameter store is ONE global table with no routing of its own, which
 * is safe only because both datapumps read every parameter inside `create`
 * and never again -- so setting them immediately before each `create`, as
 * `params_for` does, is enough.  Written down rather than left latent.
 */
static long rt_buf[NEP][CFG_BUFWORDS];
static long info_buf[NEP][CFG_BUFWORDS];

static int cfg_iodelay = CFG_IODELAY;

static struct v8_cm *
rt_cm(int ep)
{
	return (struct v8_cm *)rt_buf[ep];
}

static struct v8_dspinfo *
rt_info(int ep)
{
	return (struct v8_dspinfo *)info_buf[ep];
}

static void
params_for(int ep)
{
	harness_param_set(MDMPRM_DPRUNTIME, (long)(size_t)rt_buf[ep]);
	harness_param_set(MDMPRM_DSPINFO, (long)(size_t)info_buf[ep]);
	harness_param_set(MDMPRM_MIN_RATE, CFG_MIN_RATE);
	harness_param_set(MDMPRM_MAX_RATE, CFG_MAX_RATE);
	harness_param_set(MDMPRM_IODELAY, cfg_iodelay);
	harness_param_set(MDMPRM_AUTOMODE, 1);
}

/* --- the wire ------------------------------------------------------------ */

static short wire[NEP][WIRE_MAX];
static int wpos;
static int wire_delay = WIRE_DELAY;

static void
wire_reset(void)
{
	memset(wire, 0, sizeof(wire));
	wpos = 0;
}

/*
 * One block each way.  Both endpoints' inputs are taken from the ring BEFORE
 * either output is written back into it, which makes this exactly the
 * `wire_delay`-sample delay `t_v34conn` builds a sample at a time -- and only
 * while the delay is at least one block long, which `main` asserts.
 */
static void
wire_take(short in[NEP][FRAG])
{
	int ep, i;

	for (i = 0; i < FRAG; i++)
		for (ep = 0; ep < NEP; ep++)
			in[ep][i] = wire[ep][(wpos + i) % wire_delay];
}

static void
wire_put(short out[NEP][FRAG])
{
	int i;

	for (i = 0; i < FRAG; i++) {
		wire[0][(wpos + i) % wire_delay] = out[1][i];
		wire[1][(wpos + i) % wire_delay] = out[0][i];
	}
	wpos = (wpos + FRAG) % wire_delay;
}

/* --- the ops tables ------------------------------------------------------- */

static struct dp_operations *
ops_for(int id)
{
	int i;

	for (i = 0; i < harness_reg_ref.count; i++)
		if (harness_reg_ref.id[i] == id)
			return harness_reg_ref.ops[i];
	return 0;
}

/*
 * WHAT THE HANDOVER IS.  `v8_process` calls
 * `modem_set_param(modem, MDMPRM_DP_REQUESTED, arg)` and returns
 * DPSTAT_CHANGEDP; the core reads `dp_requested` and creates that datapump.
 *
 * AND CHANGEDP HAS TWO PRODUCERS.  The V8_OK arm asks for the negotiated
 * datapump; the idle timer expiring at the bottom of `v8_process` asks for 0
 * -- "give up and change anyway" -- and returns the same code.  A test that
 * watched only the return value would call the second a successful
 * negotiation, so the VALUE is what gets asserted.
 */
static int
first_dp_request(struct modem_shim *s, int from, int to)
{
	int i;

	for (i = from; i < to && i < HARNESS_SHIM_PARAMS; i++)
		if (s->param_name[i] == MDMPRM_DP_REQUESTED)
			return s->param_value[i];
	return -1;
}

/* --- V.8 ------------------------------------------------------------------ */

struct v8res {
	int	blocks;		/* blocks until it asked to change   */
	int	status;		/* the DPSTAT_* it returned          */
	int	requested;	/* what it asked to change TO        */
	int	f08, f0c;	/* what it published through dspinfo */
	int	b0, b1, b2;
	int	menu, offered;
};

static struct v8res v8res[NEP];
static int ep_route[NEP];

static void
run_v8(struct dp_operations *ops)
{
	struct dp *dp[NEP];
	short in[NEP][FRAG], out[NEP][FRAG];
	int done[NEP];
	int ep, blk;

	for (ep = 0; ep < NEP; ep++) {
		memset(rt_buf[ep], 0, sizeof(rt_buf[ep]));
		memset(info_buf[ep], 0, sizeof(info_buf[ep]));
		memset(&v8res[ep], 0, sizeof(v8res[ep]));
		v8res[ep].requested = -1;
		v8res[ep].status = -1;
		v8res[ep].blocks = -1;
		done[ep] = 0;
		params_for(ep);
		dp[ep] = ops->create(ep_modem[ep], DP_V34, ep_caller[ep], SRATE,
				     FRAG, ops);
		if (dp[ep] == 0)
			return;
	}

	wire_reset();
	for (blk = 0; blk < V8_MAX_BLOCKS && !(done[0] && done[1]); blk++) {
		wire_take(in);
		for (ep = 0; ep < NEP; ep++) {
			struct modem_shim *s =
				&harness_modem_route_ref[ep_route[ep]];
			int before = s->nparams;
			int rc;

			memset(out[ep], 0, sizeof(out[ep]));
			if (done[ep])
				continue;
			rc = ops->process(dp[ep], in[ep], out[ep], FRAG);
			if (rc != DPSTAT_OK) {
				done[ep] = 1;
				v8res[ep].blocks = blk;
				v8res[ep].status = rc;
				v8res[ep].requested =
					first_dp_request(s, before, s->nparams);
			}
		}
		wire_put(out);
	}

	for (ep = 0; ep < NEP; ep++) {
		v8res[ep].f08 = rt_info(ep)->f08;
		v8res[ep].f0c = rt_info(ep)->f0c;
		v8res[ep].b0 = rt_cm(ep)->b0;
		v8res[ep].b1 = rt_cm(ep)->b1;
		v8res[ep].b2 = rt_cm(ep)->b2;
		v8res[ep].menu = rt_cm(ep)->menu;
		v8res[ep].offered = rt_cm(ep)->offered;
	}

	/*
	 * `do_modem_change_dp` creates the new datapump before deleting the
	 * old one.  Nothing here depends on the order and destroying first
	 * keeps the allocation accounting readable.
	 */
	for (ep = 0; ep < NEP; ep++)
		ops->destroy(dp[ep]);
}

/* --- V.34 ----------------------------------------------------------------- */

static char *root[NEP], *obj[NEP];

static short
peek16(int ep, unsigned off)
{
	short v;

	memcpy(&v, obj[ep] + off, sizeof(v));
	return v;
}

static int
peek32(int ep, unsigned off)
{
	int v;

	memcpy(&v, obj[ep] + off, sizeof(v));
	return v;
}

struct v34res {
	int	built;
	int	role;
	int	mode0;			/* what the constructor left  */
	int	filtdelay;
	int	connect_blk;		/* first block with mode 1    */
	int	mode, txrate, rxrate;
	int	mst, rxst, txst;
	int	gets, puts, rx_len;
	int	errors, aligned, lag;	/* the bit error count        */
	int	self_locked;		/* it received its OWN pattern */
	int	repeats;		/* "Repeated info0" lines     */
};

static struct v34res v34res[NEP];

static volatile int cur_block, cur_ep;
static int trace_lo = -1, trace_hi = -1;
static int verbose;

static void
call_alarm(int sig)
{
	char msg[160];
	int n;

	(void)sig;
	n = snprintf(msg, sizeof(msg),
		     "\nt_v34link: block %d endpoint %d did not return\n",
		     cur_block, cur_ep);
	if (n > 0)
		(void)!write(2, msg, (size_t)n);
	_exit(3);
}

/*
 * THE BIT ERROR RATE, the way `t_b103link.c` measures it.
 *
 * The receiver hands up whatever was in the demodulator when data mode
 * opened, so the recovered stream starts at an unknown phase of the
 * transmitted one and after an unknown amount of junk.  This finds the first
 * offset from which the REST of the sink matches the pattern exactly, and the
 * claim is then ZERO errors from there -- not "few".  A noiseless channel
 * with no impairment at all makes one error a defect rather than bad luck.
 *
 * Returns 0 on a lock and -1 otherwise, which is the honest answer for a
 * stream that never locked; `counted` says over how many bits, so a lock
 * found in the last handful cannot pass for a measurement.
 */
static int
align_errors(const unsigned char *rx, int n, const unsigned char *pat,
	     int patlen, int *lag_out, int *counted)
{
	int lag, skip;

	*lag_out = -1;
	*counted = 0;
	for (skip = 0; skip + 1024 <= n; skip++)
		for (lag = 0; lag < patlen; lag++) {
			int i, bad = 0;

			for (i = skip; i < n; i++)
				if (rx[i] != pat[(i - skip + lag) % patlen]) {
					bad = 1;
					break;
				}
			if (!bad) {
				*lag_out = lag;
				*counted = n - skip;
				return 0;
			}
		}
	return -1;
}

static void
run_v34(struct dp_operations *ops, int nblock)
{
	struct dp *dp[NEP];
	short in[NEP][FRAG], out[NEP][FRAG];
	unsigned last[NEP];
	void (*prev)(int);
	int ep, blk;

	for (ep = 0; ep < NEP; ep++) {
		memset(&v34res[ep], 0, sizeof(v34res[ep]));
		v34res[ep].connect_blk = -1;
		last[ep] = 0xffffffffu;
		params_for(ep);
		dp[ep] = ops->create(ep_modem[ep], 34, ep_caller[ep], SRATE,
				     FRAG, ops);
		if (dp[ep] == 0)
			return;
		root[ep] = (char *)dp[ep];
		obj[ep] = root[ep] + ROOT_V34;
		v34res[ep].built = 1;
		v34res[ep].role = (unsigned short)peek16(ep, O_ROLE);
		v34res[ep].mode0 = peek32(ep, O_MODE);
	}

	prev = signal(SIGALRM, call_alarm);
	wire_reset();
	for (blk = 0; blk < nblock; blk++) {
		cur_block = blk;
		wire_take(in);
		for (ep = 0; ep < NEP; ep++) {
			const char *t;
			unsigned trip;

			cur_ep = ep;
			memset(out[ep], 0, sizeof(out[ep]));
			dsplib_debug_capture_reset();
			alarm(30);
			(void)ops->process(dp[ep], in[ep], out[ep], FRAG);
			alarm(0);

			t = dsplib_debug_capture_text(1);
			if (strstr(t, "Repeated info0") != 0)
				v34res[ep].repeats++;
			if (peek32(ep, O_MODE) == MODE_CONNECTED
			    && v34res[ep].connect_blk < 0) {
				v34res[ep].connect_blk = blk;
				v34res[ep].filtdelay = peek16(ep, O_FILTDELAY);
			}

			trip = ((unsigned)(unsigned short)peek16(ep, O_MICROSTATE)
				<< 16)
			       | ((unsigned)(unsigned char)peek16(ep, O_RXSTATE)
				  << 8)
			       | (unsigned)(unsigned char)peek16(ep, O_TXSTATE);
			if (verbose && trip != last[ep]) {
				printf("  %5d %-9s mst %3d rx %3d tx %3d  "
				       "mode %d  counter %d short_3588 %d\n", blk,
				       ep_name[ep], peek16(ep, O_MICROSTATE),
				       peek16(ep, O_RXSTATE),
				       peek16(ep, O_TXSTATE),
				       peek32(ep, O_MODE),
				       peek16(ep, O_COUNTER),
				       peek16(ep, O_F3588));
				last[ep] = trip;
			}
			if (verbose && blk >= trace_lo && blk < trace_hi
			    && t[0] != '\0')
				printf("  %5d %-9s | %.400s", blk, ep_name[ep],
				       t);
		}
		wire_put(out);
	}
	signal(SIGALRM, prev);

	for (ep = 0; ep < NEP; ep++) {
		struct modem_shim *s = &harness_modem_route_ref[ep_route[ep]];
		struct v34res *r = &v34res[ep];
		int other = ep ^ 1;
		int lag, counted;

		r->mode = peek32(ep, O_MODE);
		r->txrate = peek16(ep, O_TXRATE);
		r->rxrate = peek16(ep, O_RXRATE);
		r->mst = peek16(ep, O_MICROSTATE);
		r->rxst = peek16(ep, O_RXSTATE);
		r->txst = peek16(ep, O_TXSTATE);
		r->gets = s->gets;
		r->puts = s->puts;
		r->rx_len = s->rx_len;

		/*
		 * WHAT THIS ENDPOINT RECEIVED MUST BE THE OTHER ENDPOINT'S
		 * PATTERN.  Checking against its own would pass on a shim that
		 * had quietly handed both endpoints one stream, so BOTH are
		 * computed: the far one must lock and the near one must not.
		 */
		r->errors = align_errors(s->rx, s->rx_len, pattern[other],
					 PAT_LEN, &r->lag, &r->aligned);
		r->self_locked = align_errors(s->rx, s->rx_len, pattern[ep],
					      PAT_LEN, &lag, &counted) == 0;
	}

	for (ep = 0; ep < NEP; ep++)
		ops->destroy(dp[ep]);
}

/* --- main ----------------------------------------------------------------- */

int
main(void)
{
	struct dp_operations *ops8, *ops34;
	char msg[192];
	int rc = 0;
	int ep;
	const char *e;

	verbose = getenv("V34LINK_DUMP") != 0;
	if (verbose)
		setvbuf(stdout, NULL, _IONBF, 0);
	/*
	 * THE THREE DIAGNOSTIC KNOBS, and none of them changes what is
	 * asserted: every claim below is written against the compile-time
	 * constants.  They exist so that finding F960's mechanism and finding
	 * F962's threshold can be re-measured without editing the file.
	 */
	e = getenv("V34LINK_IODELAY");
	if (e != 0)
		cfg_iodelay = atoi(e);
	e = getenv("V34LINK_WIRE");
	if (e != 0)
		wire_delay = atoi(e);
	e = getenv("V34LINK_TRACE");
	if (e != 0 && sscanf(e, "%d:%d", &trace_lo, &trace_hi) != 2)
		trace_lo = trace_hi = -1;

	make_patterns();
	harness_alloc_reset();
	harness_param_reset();
	harness_modem_reset(0, 0);
	harness_reg_reset();
	ref_dp_v8_init();
	ref_dp_vpcm_init();
	ops8 = ops_for(DP_V8);
	ops34 = ops_for(34);

	diff_begin("the two datapumps, and the bit pipe routed per endpoint");
	diff_eq_int("dp_v8_init registered id 8", ops8 != 0, 1, 0);
	diff_eq_int("dp_vpcm_init registered id 34", ops34 != 0, 1, 0);
	if (ops8 == 0 || ops34 == 0 || ops8->create == 0 || ops34->create == 0
	    || ops8->process == 0 || ops34->process == 0 || ops34->name == 0)
		return diff_end();
	diff_eq_int("...under the name VPCM",
		    strcmp(ops34->name, "VPCM") == 0, 1, 0);
	/*
	 * The wire is a block-at-a-time version of a per-sample delay line and
	 * is only equivalent while the delay is at least one block.
	 */
	diff_eq_int("the wire is at least one block long", wire_delay >= FRAG,
		    1, wire_delay);
	diff_eq_int("...and fits the ring", wire_delay <= WIRE_MAX, 1,
		    wire_delay);

	/*
	 * THE ROUTING, AND THAT IT ROUTES.  Registering the two handles, and
	 * then the three claims that make the bit-error numbers below mean
	 * anything: the two handles differ, a duplicate registration is
	 * refused rather than silently sharing a shim, and the two patterns
	 * are not the same bits.
	 */
	for (ep = 0; ep < NEP; ep++) {
		ep_route[ep] = harness_modem_route_add(ep_modem[ep],
						       pattern[ep], PAT_LEN);
		snprintf(msg, sizeof(msg), "%s: the bit pipe took a route",
			 ep_name[ep]);
		diff_eq_int(msg, ep_route[ep], ep, ep);
	}
	diff_eq_int("the two handles are distinct",
		    ep_modem[0] != ep_modem[1], 1, 0);
	diff_eq_int("...and a repeat registration is refused",
		    harness_modem_route_add(ep_modem[0], pattern[0], PAT_LEN),
		    -1, 0);
	diff_eq_int("the two endpoints' bit patterns differ",
		    memcmp(pattern[0], pattern[1], PAT_LEN) != 0, 1, 0);
	rc |= diff_end();

	dsplib_debug_capture_on = 1;
	ref_dsplibs_debug_level = 2u;
	dsplibs_debug_level = 2u;

	/* --- V.8, and that the handover fires -------------------------- */

	diff_begin("V.8 between two endpoints, and the datapump change");
	run_v8(ops8);
	for (ep = 0; ep < NEP; ep++) {
		const struct v8res *r = &v8res[ep];

		snprintf(msg, sizeof(msg), "%s: V.8 finished", ep_name[ep]);
		diff_eq_int(msg, r->blocks >= 0, 1, ep);
		snprintf(msg, sizeof(msg), "%s: ...returning CHANGEDP",
			 ep_name[ep]);
		diff_eq_int(msg, r->status, DPSTAT_CHANGEDP, ep);
		/*
		 * AND ASKING FOR V.34 BY NUMBER.  The idle timer's expiry
		 * returns the same DPSTAT_CHANGEDP with `dp_requested` 0, so
		 * this is the claim that separates a negotiation from a
		 * timeout -- the evidence that the handover FIRED, rather than
		 * the inference that it must have.
		 */
		snprintf(msg, sizeof(msg),
			 "%s: ...and asking the modem for DP_V34", ep_name[ep]);
		diff_eq_int(msg, r->requested, DP_V34, ep);
		/*
		 * What it agreed, in the block V.34 will read.  `v8_create`
		 * sets 0x20 (V.34) and 0x80 (V.32) and clears 0x08 (V.90);
		 * bit 0 is the negotiation's own.  A run that agreed nothing
		 * would leave `b0` without 0x20 and `v8_process` would have
		 * returned DPSTAT_ERROR instead.
		 */
		snprintf(msg, sizeof(msg), "%s: the menu V.8 agreed, b0",
			 ep_name[ep]);
		diff_eq_int(msg, r->b0, 0xa1, ep);
		snprintf(msg, sizeof(msg), "%s: ...b1", ep_name[ep]);
		diff_eq_int(msg, r->b1, 0x40, ep);
		snprintf(msg, sizeof(msg), "%s: ...b2", ep_name[ep]);
		diff_eq_int(msg, r->b2, 0x00, ep);
		snprintf(msg, sizeof(msg), "%s: ...and V.34 survived in it",
			 ep_name[ep]);
		diff_eq_int(msg, (r->b0 & 0x20) != 0, 1, ep);
		/*
		 * `v8_process` publishes both of these on the V8_OK arm and
		 * nowhere else, through a block private to this endpoint --
		 * so they are a third, independent reading of "the handover
		 * fired here", one that no shared log can blur.
		 */
		snprintf(msg, sizeof(msg), "%s: dspinfo f08 published",
			 ep_name[ep]);
		diff_eq_int(msg, r->f08, 0, ep);
		snprintf(msg, sizeof(msg), "%s: dspinfo f0c published",
			 ep_name[ep]);
		diff_eq_int(msg, r->f0c, 0, ep);
	}
	/*
	 * NOT THE SAME MODEM TWICE.  The two ends differ only in `caller`, so
	 * a role that failed to take would leave two originators talking past
	 * each other -- and both would still finish, because each would then
	 * be listening for what it was itself sending.
	 */
	diff_eq_int("the two ends did not finish in the same block",
		    v8res[0].blocks != v8res[1].blocks, 1, 0);
	rc |= diff_end();

	/* --- V.34, and that it CONNECTS and carries data ---------------- */

	diff_begin("V.34 on the same runtime block: connect, and carry data");
	run_v34(ops34, V34_BLOCKS);
	for (ep = 0; ep < NEP; ep++) {
		const struct v34res *r = &v34res[ep];

		snprintf(msg, sizeof(msg), "%s: the constructor returned an "
			 "object", ep_name[ep]);
		diff_eq_int(msg, r->built, 1, ep);
		if (!r->built)
			continue;
		snprintf(msg, sizeof(msg), "%s: the role flag `caller` set",
			 ep_name[ep]);
		diff_eq_int(msg, r->role, ep_role[ep], ep);
		snprintf(msg, sizeof(msg),
			 "%s: the mode word the constructor left", ep_name[ep]);
		diff_eq_int(msg, r->mode0, 0, ep);

		/*
		 * THE CONNECTION, ASSERTED AS AN EXACT FACT.  `v34handshak`
		 * writes 1 into +0x2218 at 0x63d4d, five instructions after
		 * printing both rates, and `vpcm_run` then takes the DATA
		 * branch.  Mode 1 IS the connection.
		 */
		snprintf(msg, sizeof(msg), "%s: CONNECTED -- the mode word",
			 ep_name[ep]);
		diff_eq_int(msg, r->mode, MODE_CONNECTED, ep);
		snprintf(msg, sizeof(msg), "%s: ...the transmit rate, in units "
			 "of 2400", ep_name[ep]);
		diff_eq_int(msg, r->txrate, RATE_33600, ep);
		snprintf(msg, sizeof(msg), "%s: ...and the receive rate",
			 ep_name[ep]);
		diff_eq_int(msg, r->rxrate, RATE_33600, ep);
		snprintf(msg, sizeof(msg),
			 "%s: ...ending in DATAXMIT", ep_name[ep]);
		diff_eq_int(msg, r->txst, TXSTATE_DATAXMIT, ep);
		/*
		 * THE STARTUP DID NOT ERROR-RECOVER ONCE.  `t_v34conn`'s call,
		 * and every configuration below finding F962's threshold, reach
		 * arm 47 `TX_PHASE2_ANS`, take its "Repeated info0" branch and
		 * cycle 41/44 for ever.  Zero of them here is the whole
		 * difference between the two runs, and it is claimed exactly
		 * rather than as "few".
		 */
		snprintf(msg, sizeof(msg), "%s: ...with no error recovery at "
			 "all", ep_name[ep]);
		diff_eq_int(msg, r->repeats, 0, ep);
		/*
		 * `filtdelay` is `((iodelay + 6) >> 2) + 34` and is what arm 47 enters
		 * with; the wait it must then sit out is `0x5f - filtdelay`.
		 * Asserted because it is the quantity the whole configuration
		 * turns on -- finding F960.
		 */
		snprintf(msg, sizeof(msg),
			 "%s: ...at filtdelay ((iodelay + 6) >> 2) + 34",
			 ep_name[ep]);
		/*
		 * The object's arithmetic, not the fitted `35 + iodelay/4`
		 * this asserted -- which passed only because CFG_IODELAY is
		 * 216, a value where both agree, and is one too small when
		 * `IODELAY mod 4` is 2 or 3.  Finding F1021.
		 */
		diff_eq_int(msg, r->filtdelay,
			    ((CFG_IODELAY + 6) >> 2) + 34, ep);

		/*
		 * DATA MODE, AND BOTH DIRECTIONS AT ONCE.  `vpcm_run` is the
		 * only caller of `modem_get_bits`/`modem_put_bits` for V.34 in
		 * the whole object, and both counts are per ENDPOINT because
		 * of the routing -- so a half-duplex defect, where one
		 * endpoint carries and the other does not, cannot hide behind
		 * a pair total.
		 */
		snprintf(msg, sizeof(msg), "%s: the modem was asked for bits",
			 ep_name[ep]);
		diff_eq_int(msg, r->gets > 0, 1, r->gets);
		snprintf(msg, sizeof(msg), "%s: ...and handed bits back",
			 ep_name[ep]);
		diff_eq_int(msg, r->puts > 0, 1, r->puts);
		snprintf(msg, sizeof(msg), "%s: ...enough of them to measure a "
			 "BER on", ep_name[ep]);
		diff_eq_int(msg, r->rx_len >= 2048, 1, r->rx_len);

		/*
		 * BER 0.  `t_b103link.c`'s bar and its argument: a noiseless
		 * channel with no impairment at all, so a single error is a
		 * defect rather than bad luck.
		 */
		snprintf(msg, sizeof(msg), "%s: the received stream locked to "
			 "the FAR end's pattern, with zero errors", ep_name[ep]);
		diff_eq_int(msg, r->errors, 0, ep);
		snprintf(msg, sizeof(msg), "%s: ...over at least 2,048 bits",
			 ep_name[ep]);
		diff_eq_int(msg, r->aligned >= 2048, 1, r->aligned);
		/*
		 * AND IT IS NOT ITS OWN.  Two endpoints sharing one shim, or
		 * a wire that looped back, would lock against the near
		 * pattern; on a real link it cannot, because the two patterns
		 * differ.
		 */
		snprintf(msg, sizeof(msg), "%s: ...and NOT to its own",
			 ep_name[ep]);
		diff_eq_int(msg, r->self_locked, 0, ep);
	}
	/*
	 * AND THE TWO ENDPOINTS ARE NOT ONE MODEM TWICE.  They differ only in
	 * `caller`; a role that failed to take would connect two originators
	 * to each other and every claim above would still hold.
	 */
	diff_eq_int("both endpoints connected",
		    v34res[0].connect_blk >= 0 && v34res[1].connect_blk >= 0,
		    1, 0);
	diff_eq_int("...in different blocks",
		    v34res[0].connect_blk != v34res[1].connect_blk, 1, 0);
	rc |= diff_end();

	dsplib_debug_capture_on = 0;
	ref_dsplibs_debug_level = 0u;
	dsplibs_debug_level = 0u;

	if (verbose)
		for (ep = 0; ep < NEP; ep++)
			printf("  %-9s connect blk %d  mode %d  rates %d/%d  "
			       "mst %d rx %d tx %d  gets %d puts %d rx_len %d "
			       "lag %d over %d bits  repeats %d  self %d\n",
			       ep_name[ep], v34res[ep].connect_blk,
			       v34res[ep].mode, v34res[ep].txrate,
			       v34res[ep].rxrate, v34res[ep].mst,
			       v34res[ep].rxst, v34res[ep].txst,
			       v34res[ep].gets, v34res[ep].puts,
			       v34res[ep].rx_len, v34res[ep].lag,
			       v34res[ep].aligned, v34res[ep].repeats,
			       v34res[ep].self_locked);
	return rc;
}

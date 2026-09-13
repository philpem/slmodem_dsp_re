/*
 * t_vpcmrun.c -- the four-way comparison of a CONNECTING V.34 call.
 *
 * `t_v34link.c` established the oracle: two endpoints, one delayed wire, V.8,
 * the datapump change, the V.34 startup, 33,600 each way and BER 0 both
 * directions -- all of it the blob's own code, because `vpcm_run` was not
 * reconstructed and it is the only route to `modem_get_bits`/`modem_put_bits`
 * for V.34 (finding F963).  This file is the same call with OUR `vpcm_run` at
 * one endpoint, at the other, and at both, compared block by block against
 * the blob-blob run.
 *
 * WHAT IS AND IS NOT OURS.  `vpcm_run` is ours, and so are FOUR of the five
 * entry points it calls: `VPcmV34GetCleanedSamples` and
 * `VPcmV34GetCurrentSessionDP` are defined in `src/pump/v34/v34pcmif.c`, the
 * two rate getters in `src/pump/v34/v34pcmmain.cpp`, and this binary links
 * both.  Only `VPcmV34Progress` is still the blob's, and the one forwarder
 * below is how.  All five are declared WEAK in `src/pump/v90/vpcm.c` so that
 * a binary which supplies none of them links anyway and `vpcm_run` aborts if
 * it is called -- `t_vpcmguard.c` is the binary that watches it abort.  So
 * what is under test here is `vpcm_run`'s own work -- the block
 * quantisation, the two sample queues, the bit pipe in both directions and
 * the seventeen-arm dispatch -- plus those four callees, driven for 8,000
 * blocks of a real call.
 *
 * WHY THE COMPARISON IS PER BLOCK AND NOT ONLY AT THE END.  A run that
 * diverges at block 900 and re-converges by block 4,000 would pass every
 * final literal.  Each run records a digest of (return code, 48 output
 * samples) for every block and every endpoint, and the claim is that the
 * three mixed runs are IDENTICAL to the blob-blob one at every one of the
 * 8,000 -- with the first differing block named when they are not.
 *
 * AND THE BIT COUNTS ARE PER ENDPOINT.  Finding F965: two endpoints sharing
 * one shim give a BER that reads zero whether or not a single bit crossed the
 * wire, so each endpoint has its own route, its own pattern and its own sink,
 * and every claim below is made twice rather than once for the pair.  An
 * endpoint driven by OUR code draws from `harness_modem_route_ours[]` and one
 * driven by the blob's from `harness_modem_route_ref[]` -- the same pattern
 * in both, which is what makes the wire identical and the comparison mean
 * something.
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
#include "dsplib/vpcm.h"

extern void ref_dp_v8_init(void);
extern void ref_dp_vpcm_init(void);
extern unsigned int ref_dsplibs_debug_level;
extern unsigned int dsplibs_debug_level;

/*
 * For `run_stall` below: a SECOND, INDEPENDENT construction of the same
 * datapump, with no V.8 and no peer at all.  `dsplib/vpcm.h` already declares
 * `vpcm_create`/`vpcm_op` (ours); these are the blob's other halves.
 */
extern struct dp *ref_vpcm_create(void *modem, int id, int caller, int srate,
				  int max_frag, struct dp_operations *op);
extern struct dp_operations ref_vpcm_op;

/*
 * --- the unwritten boundary, supplied -------------------------------------
 *
 * `VPcmV34Main.cpp` is not reconstructed, so these five come from the blob.
 * They are STRONG definitions and `vpcm.c`'s declarations are weak, so the
 * link prefers these; a binary without them leaves the weak references at
 * zero and `vpcm_run`'s guard fires instead.
 */
/*
 * THE BLOB'S OWN `vpcm_run`, BY NAME.  Driving the blob arm through
 * `ops->process` would work identically -- the registered table's `.process`
 * IS this function, and that is asserted below -- but calling the `ref_`
 * alias directly is what puts `vpcm_run` in `tools/coverage.py`'s `tested`
 * set, which counts a symbol as driven against the blob only when a compiled
 * test object REFERENCES its alias.  Going through the table left the
 * coverage record saying this function was translated and untested while this
 * very file was comparing it block for block.
 */
extern int ref_vpcm_run(struct dp *dp, void *in, void *out, int count);

/* Compared with ours in the fixture block below; never called. */
extern int ref_VPcmV34Progress(void *obj, float *in, float *out, int nin,
			       int *rxbits, int *nrx, int *txbits, int *nbits);

/*
 * ALL FIVE USED TO BE FORWARDED HERE AND NONE IS NOW.  Each lost its
 * forwarder on the batch that WROTE it, because a forwarder beside a real
 * definition is a duplicate symbol: `VPcmV34GetCleanedSamples` and
 * `VPcmV34GetCurrentSessionDP` are in `src/pump/v34/v34pcmif.c`, and
 * `VPcmV34GetCurrentRxBitRate`, `VPcmV34GetCurrentTxBitRate` and
 * `VPcmV34Progress` in `src/pump/v34/v34pcmmain.cpp`.  This binary links
 * both files.
 *
 * SO THE WHOLE RUN PATH IS OURS.  The run below is no longer "our `vpcm_run`
 * on the blob's five callees" and it is no longer "four of the five are
 * ours": on a 33,600 V.34 call every instruction the OURS side executes
 * BETWEEN THE TWO SAMPLE CONVERSIONS is this tree's, and every block still
 * has to agree with the blob-blob run.  Finding F1454 is what says the claim
 * is that strong -- traced under callgrind, the call entered exactly one
 * unwritten symbol and it was `VPcmV34Progress`.
 *
 * CONSTRUCTION IS STILL BORROWED, AND DELIBERATELY.  Both endpoints are built
 * by `ref_dp_vpcm_init` and so by the blob's `ref_vpcm_create`, which is
 * findings F800-806's golden-object oracle rather than an omission: a
 * blob-constructed V.34 object is a valid differential fixture, and using it
 * is what lets the run path be compared at all.  `t_vpcmctor` and
 * `t_vpcmxfcreate` are where OUR construction is tested.  So the honest claim
 * for this file is about `.process`, not about the datapump's whole life.
 *
 * `VPcmV34Progress` also puts OUR `V90Demodulator::getBitRate`,
 * `GenericIIR<float,double>::process` and `V90Parameters::init` on the path of
 * a real connecting call, and the two rate getters already did the first.
 */

/* --- the call, and it is `t_v34link.c`'s ---------------------------------- */

#define ROOT_V34	0x2c

#define O_MODE		0x2218
#define O_MICROSTATE	0x3592
#define O_RXSTATE	0x3594
#define O_TXSTATE	0x3596
#define O_ROLE		0x359c
#define O_TXRATE	0xaa88
#define O_RXRATE	0xaa98
#define O_FILTDELAY	0xaa7c
/*
 * The one pointer finding F968's segfault turned out to be about: the
 * transmit shell context is at object +0x25e0 and `modulatevector` reads a
 * `short *` from its +0x24 at 0x5a286, indexes it at 0x5a37a and dies if it
 * is null.  Finding F986.
 */
#define O_TXSHAPE	0x2604

#define ROLE_ORIGINATE	0x65
#define ROLE_ANSWER	0x66

#define MODE_CONNECTED	1
#define RATE_33600	14
#define TXSTATE_DATAXMIT	70

#define NEP		2
#define SRATE		9600
#define FRAG		48

#define WIRE_DELAY	288
#define WIRE_MAX	8192

#define CFG_MIN_RATE	2400
#define CFG_MAX_RATE	33600
#define CFG_BUFWORDS	512

/* Finding F962: recovered from `slmodemd`'s own drivers, not chosen. */
#define CFG_IODELAY	216

#define V8_MAX_BLOCKS	3000
/* The startup takes 1,591 blocks and everything after it is data. */
#define V34_BLOCKS	4000

/*
 * The four runs.  Bit 0 is "the originator is ours", bit 1 "the answerer is".
 * Run 0 is the oracle and the other three are compared against it.
 */
#define NRUN		4
static const char *const run_name[NRUN] = {
	"blob-blob", "ours-blob", "blob-ours", "ours-ours"
};

static const char *const ep_name[NEP] = { "originate", "answer" };
static const int ep_caller[NEP] = { 1, 0 };
static const short ep_role[NEP] = { ROLE_ORIGINATE, ROLE_ANSWER };
static void *const ep_modem[NEP] = { (void *)0xD1A1u, (void *)0xD1A2u };

#define PAT_LEN	31
static unsigned char pattern[NEP][PAT_LEN];

static void
make_patterns(void)
{
	unsigned s;
	int ep, i;

	for (ep = 0; ep < NEP; ep++) {
		s = 1u + (unsigned)ep * 7919u;
		for (i = 0; i < PAT_LEN; i++) {
			s = s * 1103515245u + 12345u;
			pattern[ep][i] = (unsigned char)((s >> 19) & 1);
		}
	}
}

static long rt_buf[NEP][CFG_BUFWORDS];
static long info_buf[NEP][CFG_BUFWORDS];
/* What V.8 left behind, so all four runs start from one state. */
static long rt_snap[NEP][CFG_BUFWORDS];
static long info_snap[NEP][CFG_BUFWORDS];

static void
params_for(int ep)
{
	harness_param_set(MDMPRM_DPRUNTIME, (long)(size_t)rt_buf[ep]);
	harness_param_set(MDMPRM_DSPINFO, (long)(size_t)info_buf[ep]);
	harness_param_set(MDMPRM_MIN_RATE, CFG_MIN_RATE);
	harness_param_set(MDMPRM_MAX_RATE, CFG_MAX_RATE);
	harness_param_set(MDMPRM_IODELAY, CFG_IODELAY);
	harness_param_set(MDMPRM_AUTOMODE, 1);
}

/* --- the wire ------------------------------------------------------------ */

static short wire[NEP][WIRE_MAX];
static int wpos;

static void
wire_reset(void)
{
	memset(wire, 0, sizeof(wire));
	wpos = 0;
}

static void
wire_take(short in[NEP][FRAG])
{
	int ep, i;

	for (i = 0; i < FRAG; i++)
		for (ep = 0; ep < NEP; ep++)
			in[ep][i] = wire[ep][(wpos + i) % WIRE_DELAY];
}

static void
wire_put(short out[NEP][FRAG])
{
	int i;

	for (i = 0; i < FRAG; i++) {
		wire[0][(wpos + i) % WIRE_DELAY] = out[1][i];
		wire[1][(wpos + i) % WIRE_DELAY] = out[0][i];
	}
	wpos = (wpos + FRAG) % WIRE_DELAY;
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
 * And OURS out of our own log: `vpcm_run` is file-static in the object, so
 * the table `dp_vpcm_init` registers is the only handle on it.  `->process`
 * IS `vpcm_run` directly -- this datapump has no `dp_wrapper` -- so the mixed
 * runs below call our side through it.
 */
static struct dp_operations *
our_ops_for(int id)
{
	int i;

	for (i = 0; i < harness_reg_ours.count; i++)
		if (harness_reg_ours.id[i] == id)
			return harness_reg_ours.ops[i];
	return 0;
}

static struct dp_operations *g_our34;	/* ours, for run_v34 and run_stall */

static int ep_route[NEP];

static void
routes_reset(void)
{
	int ep;

	harness_modem_route_reset();
	for (ep = 0; ep < NEP; ep++)
		ep_route[ep] = harness_modem_route_add(ep_modem[ep],
						       pattern[ep], PAT_LEN);
}

/*
 * The shim an endpoint's bits actually went through.  An endpoint driven by
 * our code lands on side 0 and one driven by the blob's on side 1, so reading
 * the wrong table would report zero gets and zero puts for a working link --
 * which is precisely the vacuous measurement finding F965 exists to prevent,
 * arriving from the other direction.
 */
static struct modem_shim *
shim_for_ep(int run, int ep)
{
	int ours = (run >> ep) & 1;

	return ours ? &harness_modem_route_ours[ep_route[ep]]
		    : &harness_modem_route_ref[ep_route[ep]];
}

/* --- V.8 ------------------------------------------------------------------ */

static int v8_ok;

static void
run_v8(struct dp_operations *ops)
{
	struct dp *dp[NEP];
	short in[NEP][FRAG], out[NEP][FRAG];
	int done[NEP];
	int ep, blk;

	v8_ok = 0;
	for (ep = 0; ep < NEP; ep++) {
		memset(rt_buf[ep], 0, sizeof(rt_buf[ep]));
		memset(info_buf[ep], 0, sizeof(info_buf[ep]));
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
			memset(out[ep], 0, sizeof(out[ep]));
			if (done[ep])
				continue;
			if (ops->process(dp[ep], in[ep], out[ep], FRAG)
			    != DPSTAT_OK)
				done[ep] = 1;
		}
		wire_put(out);
	}
	v8_ok = done[0] && done[1];

	for (ep = 0; ep < NEP; ep++)
		ops->destroy(dp[ep]);

	memcpy(rt_snap, rt_buf, sizeof(rt_snap));
	memcpy(info_snap, info_buf, sizeof(info_snap));
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
	int	filtdelay;
	int	connect_blk;
	int	mode, txrate, rxrate;
	int	txst;
	int	gets, puts, rx_len;
	int	errors, aligned, lag;
	int	self_locked;
	int	repeats;
	/*
	 * `vpcm_run`'s OWN report to the host, which is a different event from
	 * the V.34 object reaching mode 1 and is measured separately for that
	 * reason -- see the note at the assertion.
	 */
	int	connect_rc_blk;		/* first block returning DPSTAT_CONNECT */
	int	nconnect;		/* how many blocks did                  */
	int	nerror;			/* how many returned the link-error -1  */
	/*
	 * What the connect arm told the host through `modem_set_param`, in
	 * the order it told it.  0x4444 sets MDMPRM_TX_RATE and 0x445b
	 * MDMPRM_RX_RATE, both from the rates `VPcmV34Get*BitRate` returned,
	 * and nothing else in `vpcm_run` writes a parameter on this path --
	 * so three numbers pin the whole arm.
	 */
	int	nparam;
	int	p0_name, p0_value;
	int	p1_name, p1_value;
	int	dp_id;			/* what the connect arm left in dp.id */
	/*
	 * The pointer `modulatevector` dereferences at 0x5a37a, V.34 object
	 * +0x2604 -- the transmit shell context's +0x24.  Finding F986.
	 */
	int	txshape;
	int	txshape_blk;
	/*
	 * WHICH ARMS ACTUALLY FIRED, read out of `vpcm_run`'s own state
	 * rather than inferred from its output.  Root +0x14 is the progress
	 * code the dispatch last saw; `codemask` has bit N set if code N
	 * occurred and `codeseq` is a digest of the whole 4,000-block
	 * sequence, so the four runs are compared on the dispatch's INPUT as
	 * well as on the samples it produced.
	 */
	unsigned codemask;
	unsigned codeseq;
	/* Root +0xd254 and the runtime block's +0x6c -- finding F983's pair. */
	int	extradelay;
	int	addeddelay;
};

static struct v34res v34res[NRUN][NEP];

/*
 * The per-block digest.  FNV-1a over the return code and the 48 output
 * samples, which is everything the endpoint told the world that block.
 */
static unsigned blkdig[NRUN][NEP][V34_BLOCKS];

static unsigned
digest(int rc, const short *p, int n)
{
	unsigned h = 2166136261u;
	int i;

	h = (h ^ (unsigned)rc) * 16777619u;
	for (i = 0; i < n; i++) {
		h = (h ^ (unsigned)(unsigned short)p[i]) * 16777619u;
		h = (h ^ ((unsigned)(unsigned short)p[i] >> 8)) * 16777619u;
	}
	return h;
}

static volatile int cur_block, cur_ep, cur_run;

static void
call_alarm(int sig)
{
	char msg[160];
	int n;

	(void)sig;
	n = snprintf(msg, sizeof(msg),
		     "\nt_vpcmrun: run %d block %d endpoint %d did not "
		     "return\n", cur_run, cur_block, cur_ep);
	if (n > 0)
		(void)!write(2, msg, (size_t)n);
	_exit(3);
}

/* `t_v34link.c`'s alignment, verbatim: lock, then require ZERO errors. */
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
run_v34(struct dp_operations *ops, int run)
{
	struct dp *dp[NEP];
	short in[NEP][FRAG], out[NEP][FRAG];
	void (*prev)(int);
	int ep, blk;

	cur_run = run;
	memcpy(rt_buf, rt_snap, sizeof(rt_buf));
	memcpy(info_buf, info_snap, sizeof(info_buf));
	routes_reset();

	for (ep = 0; ep < NEP; ep++) {
		memset(&v34res[run][ep], 0, sizeof(v34res[run][ep]));
		v34res[run][ep].connect_blk = -1;
		v34res[run][ep].connect_rc_blk = -1;
		v34res[run][ep].txshape_blk = -1;
		v34res[run][ep].codeseq = 2166136261u;
		params_for(ep);
		dp[ep] = ops->create(ep_modem[ep], 34, ep_caller[ep], SRATE,
				     FRAG, ops);
		if (dp[ep] == 0)
			return;
		root[ep] = (char *)dp[ep];
		obj[ep] = root[ep] + ROOT_V34;
		v34res[run][ep].built = 1;
		v34res[run][ep].role = (unsigned short)peek16(ep, O_ROLE);
	}

	prev = signal(SIGALRM, call_alarm);
	wire_reset();
	for (blk = 0; blk < V34_BLOCKS; blk++) {
		cur_block = blk;
		wire_take(in);
		for (ep = 0; ep < NEP; ep++) {
			struct v34res *r = &v34res[run][ep];
			const char *t;
			int rc, code;

			cur_ep = ep;
			memset(out[ep], 0, sizeof(out[ep]));
			dsplib_debug_capture_reset();
			alarm(30);
			/*
			 * THE ONE LINE THIS FILE EXISTS FOR.  `g_our34->process`
			 * is ours -- the registered table's `.process` IS
			 * `vpcm_run` directly -- and the other arm is the
			 * blob's, through its own registered table, on the same
			 * blob-built object.
			 */
			if ((run >> ep) & 1)
				rc = g_our34->process(dp[ep], in[ep], out[ep],
						      FRAG);
			else
				rc = ref_vpcm_run(dp[ep], in[ep], out[ep],
						  FRAG);
			alarm(0);

			blkdig[run][ep][blk] = digest(rc, out[ep], FRAG);

			code = ((struct vpcm_root *)root[ep])->status;
			if (code >= 0 && code < 32)
				r->codemask |= 1u << code;
			r->codeseq = (r->codeseq ^ (unsigned)code) * 16777619u;

			if (rc == DPSTAT_CONNECT) {
				if (r->connect_rc_blk < 0)
					r->connect_rc_blk = blk;
				r->nconnect++;
			} else if (rc == -1) {
				r->nerror++;
			}

			t = dsplib_debug_capture_text(1);
			if (strstr(t, "Repeated info0") != 0)
				r->repeats++;
			if (peek32(ep, O_TXSHAPE) != 0 && r->txshape_blk < 0)
				r->txshape_blk = blk;
			if (peek32(ep, O_MODE) == MODE_CONNECTED
			    && r->connect_blk < 0) {
				r->connect_blk = blk;
				r->filtdelay = peek16(ep, O_FILTDELAY);
			}
		}
		wire_put(out);
	}
	signal(SIGALRM, prev);

	for (ep = 0; ep < NEP; ep++) {
		struct modem_shim *s = shim_for_ep(run, ep);
		struct v34res *r = &v34res[run][ep];
		int other = ep ^ 1;
		int lag, counted;

		r->mode = peek32(ep, O_MODE);
		r->txrate = peek16(ep, O_TXRATE);
		r->rxrate = peek16(ep, O_RXRATE);
		r->txst = peek16(ep, O_TXSTATE);
		r->gets = s->gets;
		r->puts = s->puts;
		r->rx_len = s->rx_len;
		r->dp_id = ((struct dp *)root[ep])->id;
		r->txshape = peek32(ep, O_TXSHAPE);
		r->extradelay = ((struct vpcm_root *)root[ep])->extradelay;
		memcpy(&r->addeddelay,
		       (const char *)rt_buf[ep]
		       + __builtin_offsetof(struct _tagModemParameters,
					    addedDelay),
		       sizeof(r->addeddelay));
		r->nparam = s->nparams;
		if (s->nparams > 0) {
			r->p0_name = (int)s->param_name[0];
			r->p0_value = s->param_value[0];
		}
		if (s->nparams > 1) {
			r->p1_name = (int)s->param_name[1];
			r->p1_value = s->param_value[1];
		}
		r->errors = align_errors(s->rx, s->rx_len, pattern[other],
					 PAT_LEN, &r->lag, &r->aligned);
		r->self_locked = align_errors(s->rx, s->rx_len, pattern[ep],
					      PAT_LEN, &lag, &counted) == 0;
	}

	for (ep = 0; ep < NEP; ep++)
		ops->destroy(dp[ep]);
}

/*
 * ===========================================================================
 * `run_stall` -- `s->stall`'s two mutations, neither of which the 8,000-block
 * connecting call above ever drives: a real 33,600 connect makes progress,
 * so it never accumulates 3,000 unchanged `VPCM_PROG_RESTART_P2` (0) blocks
 * and it never revisits code 0 after leaving it.
 *
 * NO PEER AND NO WIRE.  `vpcm_run` reaches the dispatch once `nproc > 0` and
 * the mute counter has run out (`vpcm_create` seeds it at 528 samples, 11
 * blocks); nothing about that needs a far end, so two independent objects --
 * ours from `vpcm_create`, the blob's from `ref_vpcm_create` -- are compared
 * directly, exactly as `t_vpcmdp.c`'s construction fixture compares them,
 * with `vpcm_run` added on top.
 *
 * SILENCE DOES NOT SIT AT CODE 0 FOREVER, AND THAT IS WHY THIS POKES FIELDS
 * RATHER THAN RUNNING 3,000 REAL BLOCKS.  Measured by tracing `rb->status`
 * and `rb->stall`: a genuinely silent line reports `VPCM_PROG_RESTART_P2`
 * for only about 150 blocks before `VPcmV34Progress`'s OWN internal give-up
 * reports a FAIL code and `mode` goes to ERROR that way -- a real but
 * DIFFERENT path, which would reach `VPCM_MODE_ERROR` without either stall
 * mutation being tested at all.  A loud out-of-band tone doesn't move `prog`
 * off 0 either (same trace).  So both claims below are tested by writing
 * `s->status` and `s->stall` directly, exactly as `t_vpcmguard.c`'s
 * `root_reset` pokes `obj->status` to steer a dispatch without driving the
 * signal that would naturally produce it -- the precondition is injected,
 * never the branch: every dispatch below still runs on `vpcm_run`'s own
 * code, from a REAL `prog` a REAL call computed.
 *
 * ORDER MATTERS: both pokes happen well before block ~150, while the line
 * is still genuinely reporting RESTART_P2 on its own.
 */
static int
run_stall(void)
{
	/* An opaque handle; the fake modem shim keys off it, not off contents. */
	void *const modem = (void *)0xD1A3u;
	struct dp *da, *db;
	short in[FRAG], out_a[FRAG], out_b[FRAG];
	struct vpcm_root *ra, *rb;
	int blk;
	int rc = 0;

	harness_modem_reset(0, 0);
	da = g_our34->create(modem, VPCM_DP_V34, 1, VPCM_SRATE,
				     VPCM_MAX_FRAG, g_our34);
	db = ref_vpcm_create(modem, VPCM_DP_V34, 1, VPCM_SRATE, VPCM_MAX_FRAG,
			     &ref_vpcm_op);

	diff_begin("vpcm_run: the stall counter's reset and its deadline");
	diff_eq_int("our vpcm_create returned an object", da != 0, 1, 0);
	diff_eq_int("the blob's did too", db != 0, 1, 0);
	if (da == 0 || db == 0) {
		rc |= diff_end();
		return rc;
	}
	ra = (struct vpcm_root *)da->dp_data;
	rb = (struct vpcm_root *)db->dp_data;

	memset(in, 0, sizeof(in));

	/*
	 * Blocks 0..49: run the line in genuinely, so `stall` is built up by
	 * the object's own code and not by a poke -- 11 muted, then ~38 real
	 * silent blocks accumulating `stall` under `VPCM_PROG_RESTART_P2`.
	 */
	for (blk = 0; blk < 50; blk++) {
		int sa, sb;
		char msg[96];

		memset(out_a, 0x5a, sizeof(out_a));
		memset(out_b, 0x5a, sizeof(out_b));
		sa = g_our34->process(da, in, out_a, FRAG);
		sb = ref_vpcm_run(db, in, out_b, FRAG);

		snprintf(msg, sizeof(msg), "block %d: return code (%%ld)", blk);
		diff_eq_int(msg, sa, sb, blk);
		snprintf(msg, sizeof(msg), "block %d: stall counter (%%ld)", blk);
		diff_eq_int(msg, ra->stall, rb->stall, blk);
	}
	diff_eq_int("...and the count is really moving by block 49",
		    rb->stall > 0, 1, 0);

	/*
	 * THE RESET MUTATION.  Fake "something else was just reported" on
	 * both sides, then run one more real (still-silent) block.  `prog`
	 * is still genuinely 0 here, so `s->status(99) != prog(0)` is a real
	 * comparison and the object's own dispatch takes `case
	 * VPCM_PROG_RESTART_P2` on its own terms.  Without the reset, `stall`
	 * keeps the count built up over blocks 11-49 instead of restarting;
	 * WITH it, both sides read 0 right after this call.
	 */
	ra->status = 99;
	rb->status = 99;
	memset(out_a, 0x5a, sizeof(out_a));
	memset(out_b, 0x5a, sizeof(out_b));
	g_our34->process(da, in, out_a, FRAG);
	ref_vpcm_run(db, in, out_b, FRAG);
	diff_eq_int("the reset ran on the blob's side (stall back to 0)",
		    rb->stall, 0, 0);
	diff_eq_int("...and ours agrees (the reset mutation's claim)",
		    ra->stall, rb->stall, 0);

	/*
	 * THE OFF-BY-ONE MUTATION.  `status` is already back at 0 (the
	 * dispatch's own tail assigns it), so poking `stall` to one below the
	 * deadline and running one more real silent block lands EXACTLY on
	 * the boundary `>` (correct) and `>=` (mutant) disagree on: `stall`
	 * becomes `VPCM_TRAIN_TIMEOUT` (3000) after this call, which is not
	 * `> 3000` and is `>= 3000`.  A second call then crosses the boundary
	 * both readings agree is past it, so the blob's own trajectory is
	 * checked at both steps rather than asserted from arithmetic alone.
	 */
	ra->stall = VPCM_TRAIN_TIMEOUT - 1;
	rb->stall = VPCM_TRAIN_TIMEOUT - 1;
	memset(out_a, 0x5a, sizeof(out_a));
	memset(out_b, 0x5a, sizeof(out_b));
	g_our34->process(da, in, out_a, FRAG);
	ref_vpcm_run(db, in, out_b, FRAG);
	diff_eq_int("at stall == TIMEOUT exactly, the blob has NOT given up",
		    rb->mode, VPCM_MODE_IDLE, 0);
	diff_eq_int("...and ours agrees (the off-by-one mutation's claim)",
		    ra->mode, rb->mode, 0);
	diff_eq_int("...stall itself still agrees too", ra->stall, rb->stall, 0);

	memset(out_a, 0x5a, sizeof(out_a));
	memset(out_b, 0x5a, sizeof(out_b));
	g_our34->process(da, in, out_a, FRAG);
	ref_vpcm_run(db, in, out_b, FRAG);
	diff_eq_int("one block later, past TIMEOUT, the blob HAS given up",
		    rb->mode, VPCM_MODE_ERROR, 0);
	diff_eq_int("...and ours agrees", ra->mode, rb->mode, 0);

	rc |= diff_end();
	return rc;
}

/* --- main ----------------------------------------------------------------- */

int
main(void)
{
	struct dp_operations *ops8, *ops34;
	char msg[224];
	int rc = 0;
	int run, ep;
	int verbose = getenv("VPCMRUN_DUMP") != 0;

	make_patterns();
	harness_alloc_reset();
	harness_param_reset();
	harness_modem_reset(0, 0);
	harness_reg_reset();
	ref_dp_v8_init();
	ref_dp_vpcm_init();
	dp_vpcm_init();
	ops8 = ops_for(DP_V8);
	ops34 = ops_for(34);
	g_our34 = our_ops_for(34);

	diff_begin("the fixture: two datapumps, two routes, and the "
		   "unwritten boundary supplied");
	diff_eq_int("dp_v8_init registered id 8", ops8 != 0, 1, 0);
	diff_eq_int("dp_vpcm_init registered id 34", ops34 != 0, 1, 0);
	diff_eq_int("our dp_vpcm_init registered id 34", g_our34 != 0, 1, 0);
	if (ops8 == 0 || ops34 == 0 || ops8->create == 0 || ops34->create == 0
	    || ops8->process == 0 || ops34->process == 0
	    || g_our34 == 0 || g_our34->process == 0)
		return diff_end();
	/*
	 * THAT THE FIVE ENTRY POINTS ARE PRESENT IN THIS BINARY IS NOT
	 * ASSERTED HERE, and the reason is worth writing down because the
	 * first version of this file did assert it and the assertion was
	 * vacuous: this translation unit DEFINES all five, so `f != 0` is a
	 * constant the compiler folds and the check would pass in a binary
	 * where they had never been called.
	 *
	 * The claim that actually carries it is `vpcm_unwritten()` after the
	 * four runs, below -- `vpcm_run` records the first entry point it
	 * could not call, so a zero there says every one of them was reached
	 * for real.  `t_vpcmguard.c` makes the complementary claim, in a
	 * binary that defines none of them and declares them weak so that the
	 * comparison is a comparison.
	 */
	/*
	 * AND THE BLOB ARM REALLY IS THE DATAPUMP'S `.process`.  The runs
	 * below call `ref_vpcm_run` by name rather than through the table, so
	 * this is what keeps that honest -- and it is finding F963's
	 * identification of which entry carries the payload, checked rather
	 * than quoted.
	 */
	diff_eq_int("the VPCM table's .process IS vpcm_run",
		    (void *)ops34->process == (void *)ref_vpcm_run, 1, 0);
	diff_eq_int("...under the name VPCM",
		    ops34->name != 0 && strcmp(ops34->name, "VPCM") == 0, 1,
		    0);
	/*
	 * AND OUR `VPcmV34Progress` IS NOT THE BLOB'S.  It became a real
	 * definition in `src/pump/v34/v34pcmmain.cpp` on the batch that
	 * removed this file's forwarder, and the two are now two functions:
	 * a link that had somehow resolved ours to the blob's copy would make
	 * every comparison below trivially true and nothing else would say so.
	 *
	 * IT IS ALSO WHAT PUTS IT IN `tools/coverage.py`'s `tested` SET,
	 * which counts a symbol as driven against the blob only when a
	 * compiled test object REFERENCES its `ref_` alias.  This binary
	 * drives it for 8,000 blocks at two endpoints -- but through
	 * `ref_vpcm_run`, one call deeper than the tool can see, so without
	 * this line the record would read `translated, alias exists, and NOT
	 * tested` for the one function this file exists to test.  The same
	 * indirection is why `ref_vpcm_run` is called by name above.
	 */
	diff_eq_int("our VPcmV34Progress is not the blob's",
		    (void *)VPcmV34Progress != (void *)ref_VPcmV34Progress, 1,
		    0);
	diff_eq_int("the root object is vpcm_create's allocation",
		    (int)sizeof(struct vpcm_root), 0xd258, 0);
	diff_eq_int("the wire is at least one block long",
		    WIRE_DELAY >= FRAG, 1, WIRE_DELAY);
	rc |= diff_end();

	dsplib_debug_capture_on = 1;
	ref_dsplibs_debug_level = 2u;
	dsplibs_debug_level = 2u;

	routes_reset();
	run_v8(ops8);

	diff_begin("V.8 ran, and left one starting state for all four runs");
	diff_eq_int("both endpoints finished V.8", v8_ok, 1, 0);
	diff_eq_int("...and V.34 survived in the agreed menu",
		    (((unsigned char *)rt_snap[0])[0] & 0x20) != 0, 1, 0);

	for (run = 0; run < NRUN; run++)
		run_v34(ops34, run);

	/*
	 * NOTHING UNWRITTEN WAS REACHED.  `vpcm_run` records the first
	 * unwritten entry point it takes; a zero here says the four runs went
	 * through the real ones and not through a guard that returned.
	 */
	diff_eq_int("no unwritten path was taken", vpcm_unwritten(),
		    VPCM_WRITTEN, 0);
	rc |= diff_end();

	/* --- the oracle, as exact literals ----------------------------- */

	diff_begin("the blob-blob oracle: connect, and carry data both ways");
	for (ep = 0; ep < NEP; ep++) {
		const struct v34res *r = &v34res[0][ep];

		snprintf(msg, sizeof(msg), "%s: built", ep_name[ep]);
		diff_eq_int(msg, r->built, 1, ep);
		snprintf(msg, sizeof(msg), "%s: the role flag `caller` set",
			 ep_name[ep]);
		diff_eq_int(msg, r->role, ep_role[ep], ep);
		snprintf(msg, sizeof(msg), "%s: CONNECTED -- the mode word",
			 ep_name[ep]);
		diff_eq_int(msg, r->mode, MODE_CONNECTED, ep);
		snprintf(msg, sizeof(msg), "%s: ...at 33,600 transmit",
			 ep_name[ep]);
		diff_eq_int(msg, r->txrate, RATE_33600, ep);
		snprintf(msg, sizeof(msg), "%s: ...and 33,600 receive",
			 ep_name[ep]);
		diff_eq_int(msg, r->rxrate, RATE_33600, ep);
		snprintf(msg, sizeof(msg), "%s: ...ending in DATAXMIT",
			 ep_name[ep]);
		diff_eq_int(msg, r->txst, TXSTATE_DATAXMIT, ep);
		snprintf(msg, sizeof(msg), "%s: ...with no error recovery",
			 ep_name[ep]);
		diff_eq_int(msg, r->repeats, 0, ep);
		/*
		 * THE OBJECT'S ARITHMETIC, NOT THE FITTED FORM.  This asserted
		 * `35 + iodelay/4` and passed only because CFG_IODELAY is 216,
		 * one of the values where the two agree.  `VPcmV34SetDelays`
		 * computes `((hwDelay + 2) >> 2) + 0x22`, i.e.
		 * `((IODELAY + 6) >> 2) + 34`, and the fitted form is one too
		 * small whenever `IODELAY mod 4` is 2 or 3 -- at 150 the object
		 * gives 73 and the fit gives 72.  Finding F1021.
		 *
		 * It matters beyond tidiness: at IODELAY 86 the object gives
		 * 57, which IS the threshold, and the fit gives 56.  That is
		 * the whole of the 86-versus-88 boundary correction.
		 */
		snprintf(msg, sizeof(msg),
			 "%s: ...at filtdelay ((iodelay + 6) >> 2) + 34",
			 ep_name[ep]);
		diff_eq_int(msg, r->filtdelay,
			    ((CFG_IODELAY + 6) >> 2) + 34, ep);
		/*
		 * `vpcm_run`'s OWN report to the host, and it is a DIFFERENT
		 * EVENT from the V.34 object reaching mode 1 -- and it is the
		 * EARLIER one, by seven blocks and eight.  The object's
		 * +0x2218 is written by `v34handshak` at 0x63d4d; `vpcm_run`
		 * reports DPSTAT_CONNECT when `VPcmV34Progress` hands it a
		 * progress code of 4 or 5, which it does first.  The first
		 * version of this file asserted they were the same block, and
		 * the BLOB-BLOB oracle failed it -- which is the only reason
		 * the distinction is recorded here rather than assumed away,
		 * and the direction of the gap is why it is asserted as an
		 * ordering rather than only as two literals.
		 *
		 * ONCE, AND EXACTLY ONCE: the report is edge-triggered off a
		 * mode CHANGE, so a modem that connected would say so in one
		 * block and a `vpcm_run` that had lost the edge would say it
		 * in every block after.  The count is the claim, not "> 0".
		 */
		snprintf(msg, sizeof(msg),
			 "%s: .process reported DPSTAT_CONNECT exactly once",
			 ep_name[ep]);
		diff_eq_int(msg, r->nconnect, 1, ep);
		snprintf(msg, sizeof(msg),
			 "%s: ...and never reported a link error",
			 ep_name[ep]);
		diff_eq_int(msg, r->nerror, 0, ep);
		snprintf(msg, sizeof(msg),
			 "%s: ...BEFORE the object's own mode word turned 1",
			 ep_name[ep]);
		diff_eq_int(msg, r->connect_rc_blk < r->connect_blk, 1,
			    r->connect_rc_blk);
		/*
		 * AND WHAT IT TOLD THE HOST.  The connect arm's whole visible
		 * effect on the world outside the object is two
		 * `modem_set_param` calls, and both the ORDER and the values
		 * are the claim: transmit first (0x4444), receive second
		 * (0x445b), each at 33,600.  Two parameters and no more, so a
		 * report that fired every block after the edge is caught here
		 * as well as by the count above.
		 */
		/*
		 * AND THE DATAPUMP ID IT REWROTE.  0x4422 stores 0x22 -- 34 --
		 * into the root's own `dp.id` on every session type that is
		 * not V.90 or V.92, and the root IS the `struct dp` the host
		 * holds.  `vpcm_create` was called with 34, so the store is a
		 * no-op in value here and is asserted anyway: it is the only
		 * evidence that the arm's three-way choice took the `else`
		 * rather than one of the two PCM branches.
		 */
		snprintf(msg, sizeof(msg),
			 "%s: ...and dp.id says V.34, not V.90 or V.92",
			 ep_name[ep]);
		diff_eq_int(msg, r->dp_id, VPCM_DP_V34, ep);
		/*
		 * THE POINTER FINDING F968's SEGFAULT WAS ABOUT, asserted here
		 * because it is the state a driver has to reach and the
		 * direct-drive fixture does not.  `modulatevector` loads a
		 * `short *` out of the transmit shell context's +0x24 at
		 * 0x5a286 and indexes it at 0x5a37a; with it null the
		 * process dies the moment the object takes the DATA branch,
		 * which is finding F968's block 21,707 exactly.  In a call
		 * that connects it is installed 30 and 28 blocks BEFORE the
		 * mode word turns 1, i.e. inside the handshake's last phase
		 * and not on the data branch.
		 */
		snprintf(msg, sizeof(msg),
			 "%s: the modulator's shaping pointer is installed",
			 ep_name[ep]);
		diff_eq_int(msg, r->txshape != 0, 1, ep);
		snprintf(msg, sizeof(msg),
			 "%s: ...before the mode word turns 1", ep_name[ep]);
		diff_eq_int(msg, r->txshape_blk < r->connect_blk, 1,
			    r->txshape_blk);
		/*
		 * WHICH ARMS FIRED, as a literal, read out of root +0x14
		 * every block rather than inferred from the samples.  0x1f is
		 * codes 0, 1, 2, 3 and 4: "re-starting phase II", "phase II
		 * completed", the two that do nothing, and the connect.  So
		 * five of the seventeen are exercised by this call and the
		 * other twelve -- including 10's "Same Line Verification
		 * Status" and all three link-error codes -- are written from
		 * the disassembly and reached by nothing here.
		 */
		snprintf(msg, sizeof(msg),
			 "%s: the progress codes this call produced",
			 ep_name[ep]);
		diff_eq_int(msg, (int)r->codemask, 0x1f, ep);
		/*
		 * AND WHY ARMS 0 AND 1 LEAVE NO TRACE THOUGH THEY RAN.  Both
		 * phase-II arms are gated on root +0xd254 being non-zero, and
		 * in this configuration it is zero -- so the bodies that ask
		 * the host to move the delay never execute, and `addedDelay`
		 * is still what `vpcm_create` left.  Asserted so that "arm 0
		 * fired" is not read as "the delay adjustment is tested".
		 */
		snprintf(msg, sizeof(msg),
			 "%s: ...with the phase-II delay adjustment at zero",
			 ep_name[ep]);
		diff_eq_int(msg, r->extradelay, 0, ep);
		snprintf(msg, sizeof(msg),
			 "%s: ...so nothing was added to the host's delay",
			 ep_name[ep]);
		diff_eq_int(msg, r->addeddelay, 0, ep);
		snprintf(msg, sizeof(msg),
			 "%s: the connect arm wrote two parameters",
			 ep_name[ep]);
		diff_eq_int(msg, r->nparam, 2, ep);
		snprintf(msg, sizeof(msg),
			 "%s: ...MDMPRM_TX_RATE at 33,600 first", ep_name[ep]);
		diff_eq_int(msg, r->p0_name * 100000 + r->p0_value,
			    MDMPRM_TX_RATE * 100000 + 33600, ep);
		snprintf(msg, sizeof(msg),
			 "%s: ...then MDMPRM_RX_RATE at 33,600", ep_name[ep]);
		diff_eq_int(msg, r->p1_name * 100000 + r->p1_value,
			    MDMPRM_RX_RATE * 100000 + 33600, ep);
	}
	diff_eq_int("the originator's mode word turns 1 in block 1591",
		    v34res[0][0].connect_blk, 1591, 0);
	diff_eq_int("the answerer's turns 1 in block 1590",
		    v34res[0][1].connect_blk, 1590, 0);
	diff_eq_int("the originator installs the shaping pointer in block 1561",
		    v34res[0][0].txshape_blk, 1561, 0);
	diff_eq_int("the answerer in block 1562",
		    v34res[0][1].txshape_blk, 1562, 0);
	diff_eq_int("the originator reports DPSTAT_CONNECT in block 1584",
		    v34res[0][0].connect_rc_blk, 1584, 0);
	diff_eq_int("the answerer reports it in block 1582",
		    v34res[0][1].connect_rc_blk, 1582, 0);
	rc |= diff_end();

	/* --- and the same call through OUR vpcm_run --------------------- */

	diff_begin("ours at one endpoint, at the other, and at both");
	for (run = 0; run < NRUN; run++) {
		for (ep = 0; ep < NEP; ep++) {
			const struct v34res *r = &v34res[run][ep];
			int first = -1;
			int blk;

			for (blk = 0; blk < V34_BLOCKS; blk++)
				if (blkdig[run][ep][blk]
				    != blkdig[0][ep][blk]) {
					first = blk;
					break;
				}
			/*
			 * THE FOUR-WAY COMPARISON.  4,000 blocks of return
			 * code and output samples, per endpoint, against the
			 * oracle.  `first` is the block, so a failure names
			 * where rather than only that.
			 */
			snprintf(msg, sizeof(msg),
				 "%s %s: every block identical to blob-blob",
				 run_name[run], ep_name[ep]);
			diff_eq_int(msg, first, -1, first);

			/* And the literals, run by run, endpoint by endpoint. */
			snprintf(msg, sizeof(msg), "%s %s: CONNECTED",
				 run_name[run], ep_name[ep]);
			diff_eq_int(msg, r->mode, MODE_CONNECTED, ep);
			snprintf(msg, sizeof(msg), "%s %s: ...in the same block",
				 run_name[run], ep_name[ep]);
			diff_eq_int(msg, r->connect_blk,
				    v34res[0][ep].connect_blk, ep);
			snprintf(msg, sizeof(msg), "%s %s: ...at 33,600 each way",
				 run_name[run], ep_name[ep]);
			diff_eq_int(msg, r->txrate * 100 + r->rxrate,
				    RATE_33600 * 100 + RATE_33600, ep);
			snprintf(msg, sizeof(msg),
				 "%s %s: ...reporting DPSTAT_CONNECT once, in "
				 "the same block", run_name[run], ep_name[ep]);
			diff_eq_int(msg,
				    r->nconnect * 100000 + r->connect_rc_blk,
				    100000 + v34res[0][ep].connect_rc_blk, ep);
			snprintf(msg, sizeof(msg),
				 "%s %s: ...and the same two parameters",
				 run_name[run], ep_name[ep]);
			diff_eq_int(msg,
				    r->nparam * 1000000000 + r->p0_name * 100000
				    + r->p0_value,
				    2 * 1000000000 + MDMPRM_TX_RATE * 100000
				    + 33600, ep);
			snprintf(msg, sizeof(msg),
				 "%s %s: ...in the same order",
				 run_name[run], ep_name[ep]);
			diff_eq_int(msg, r->p1_name * 100000 + r->p1_value,
				    MDMPRM_RX_RATE * 100000 + 33600, ep);
			snprintf(msg, sizeof(msg),
				 "%s %s: ...and the same dp.id",
				 run_name[run], ep_name[ep]);
			diff_eq_int(msg, r->dp_id, VPCM_DP_V34, ep);
			snprintf(msg, sizeof(msg),
				 "%s %s: ...and drove the object as far as the "
				 "shaping pointer, in the same block",
				 run_name[run], ep_name[ep]);
			diff_eq_int(msg, r->txshape_blk,
				    v34res[0][ep].txshape_blk, ep);
			/*
			 * THE DISPATCH'S INPUT, NOT ONLY ITS OUTPUT.  The
			 * digest above sees `vpcm_run`'s return code and its
			 * samples; this sees the progress code it dispatched
			 * on in every one of the 4,000 blocks, which is the
			 * state the seventeen arms exist to maintain.
			 */
			snprintf(msg, sizeof(msg),
				 "%s %s: the same progress codes, in the same "
				 "order, all 4,000 blocks",
				 run_name[run], ep_name[ep]);
			diff_eq_int(msg, (int)r->codeseq,
				    (int)v34res[0][ep].codeseq, ep);
			snprintf(msg, sizeof(msg),
				 "%s %s: ...and the same set of them",
				 run_name[run], ep_name[ep]);
			diff_eq_int(msg, (int)r->codemask, 0x1f, ep);

			/*
			 * THE BIT PIPE, PER ENDPOINT AND WITH ITS COUNT.
			 * Finding F965's trap in one line: a BER of zero over
			 * zero bits is zero, so the number of bits is asserted
			 * beside the number of errors, and never for the pair
			 * jointly.
			 */
			snprintf(msg, sizeof(msg),
				 "%s %s: the modem was asked for bits",
				 run_name[run], ep_name[ep]);
			diff_eq_int(msg, r->gets, v34res[0][ep].gets, ep);
			snprintf(msg, sizeof(msg),
				 "%s %s: ...and handed bits back",
				 run_name[run], ep_name[ep]);
			diff_eq_int(msg, r->puts, v34res[0][ep].puts, ep);
			snprintf(msg, sizeof(msg),
				 "%s %s: ...at least 2,048 of them",
				 run_name[run], ep_name[ep]);
			diff_eq_int(msg, r->rx_len >= 2048, 1, r->rx_len);
			snprintf(msg, sizeof(msg),
				 "%s %s: locked to the FAR pattern with ZERO "
				 "errors", run_name[run], ep_name[ep]);
			diff_eq_int(msg, r->errors, 0, ep);
			snprintf(msg, sizeof(msg),
				 "%s %s: ...over at least 2,048 bits",
				 run_name[run], ep_name[ep]);
			diff_eq_int(msg, r->aligned >= 2048, 1, r->aligned);
			snprintf(msg, sizeof(msg),
				 "%s %s: ...and NOT to its own",
				 run_name[run], ep_name[ep]);
			diff_eq_int(msg, r->self_locked, 0, ep);
		}
	}
	/*
	 * AND THE MIXED RUNS REALLY WERE MIXED.  Every claim above would hold
	 * if `run` had been ignored and the blob driven four times, so the
	 * selector itself is asserted: an endpoint's bits went through the
	 * OURS shim exactly when that endpoint was ours, and the ref shim
	 * otherwise.  Without this the file is four copies of `t_v34link`.
	 */
	for (run = 0; run < NRUN; run++)
		for (ep = 0; ep < NEP; ep++) {
			int ours = (run >> ep) & 1;

			snprintf(msg, sizeof(msg),
				 "%s %s: the bits went through the %s shim",
				 run_name[run], ep_name[ep],
				 ours ? "ours" : "ref");
			diff_eq_int(msg, v34res[run][ep].gets > 0, 1, ours);
		}
	rc |= diff_end();

	dsplib_debug_capture_on = 0;
	ref_dsplibs_debug_level = 0u;
	dsplibs_debug_level = 0u;

	if (verbose)
		for (run = 0; run < NRUN; run++)
			for (ep = 0; ep < NEP; ep++)
				printf("  %-9s %-9s connect %d shape %d rc blk %d x%d "
				       "err %d  mode %d  rates %d/%d  tx %d  "
				       "gets %d puts %d rx_len %d  lag %d "
				       "over %d  repeats %d self %d\n",
				       run_name[run], ep_name[ep],
				       v34res[run][ep].connect_blk,
				       v34res[run][ep].txshape_blk,
				       v34res[run][ep].connect_rc_blk,
				       v34res[run][ep].nconnect,
				       v34res[run][ep].nerror,
				       v34res[run][ep].mode,
				       v34res[run][ep].txrate,
				       v34res[run][ep].rxrate,
				       v34res[run][ep].txst,
				       v34res[run][ep].gets,
				       v34res[run][ep].puts,
				       v34res[run][ep].rx_len,
				       v34res[run][ep].lag,
				       v34res[run][ep].aligned,
				       v34res[run][ep].repeats,
				       v34res[run][ep].self_locked),
				printf("      codes %#x seq %#x\n",
				       v34res[run][ep].codemask,
				       v34res[run][ep].codeseq);

	rc |= run_stall();

	return rc;
}

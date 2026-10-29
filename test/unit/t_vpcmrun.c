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
 * WHAT IS AND IS NOT OURS.  `vpcm_run` is ours, and so are ALL FIVE entry
 * points it calls: `VPcmV34GetCleanedSamples` and
 * `VPcmV34GetCurrentSessionDP` are defined in `src/pump/v34/v34pcmif.c`, and
 * `VPcmV34Progress` with the two rate getters in
 * `src/pump/v34/v34pcmmain.cpp` / `VPcmV34Main.cpp`.  The five are plain
 * declarations in `vpcm.h` and hard link requirements, exactly as the blob's
 * direct `R_386_PC32` calls make them; the old weak-reference-plus-guard
 * idiom was added apparatus and is gone.  So what is under test here is
 * `vpcm_run`'s own work -- the block quantisation, the two sample queues, the
 * bit pipe in both directions and the seventeen-arm dispatch -- plus all five
 * callees, driven for 8,000 blocks of a real call.
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
#include "dsplib/v34fsk.h"
#include "dsplib/int_complex.h"
#include "dsplib/v34pcmif.h"

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
 * --- the blob's own half, by name -----------------------------------------
 *
 * `ref_vpcm_run` and the `ref_*` alias of `VPcmV34Progress` are how the
 * oracle arm is driven and compared.  The five real entry points are this
 * tree's now and are hard link requirements, so there is no weak boundary to
 * supply here any more.
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
 * The original four processing runs retain reference construction.  A separate
 * lifecycle cohort below uses matched create/process/destroy implementations
 * at each endpoint. Those cohorts and the visual cohort borrow reference V.8
 * state. The handoff cohort follows source V.8 publication and destruction
 * into reference/source V.34 owners using the same host buffers. All retain
 * the existing wire reset at the sequential API boundary; this does not test
 * a continuous-wire modem replacement scheduler.
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

static struct dp_operations *g_our8;
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

struct v8_block {
	short samples[FRAG];
	int result, idle_timer, last_status, nparams, parameter, request, other_params;
	struct v8_cm menu;
	struct dsp_info info;
};
static struct v8_block v8_oracle[NEP][V8_MAX_BLOCKS];
static int v8_active[NEP][V8_MAX_BLOCKS], v8_calls[NEP];
static long v8_rt_before_destroy[NEP][CFG_BUFWORDS];
static long v8_info_before_destroy[NEP][CFG_BUFWORDS];

static void
capture_v8(struct v8_block *record, struct dp *dp, int ep, int ours, int result, const short *out)
{
	struct v8_dp *v = (struct v8_dp *)dp;
	struct modem_shim *used = ours ? &harness_modem_route_ours[ep_route[ep]] : &harness_modem_route_ref[ep_route[ep]];
	struct modem_shim *other = ours ? &harness_modem_route_ref[ep_route[ep]] : &harness_modem_route_ours[ep_route[ep]];
	memset(record, 0, sizeof(*record));
	memcpy(record->samples, out, sizeof(record->samples));
	record->result = result;
	record->idle_timer = v->idle_timer;
	record->last_status = v->last_status;
	record->nparams = used->nparams;
	record->other_params = other->nparams;
	record->parameter = record->request = -1;
	if (used->nparams) {
		record->parameter = used->param_name[used->nparams - 1];
		record->request = used->param_value[used->nparams - 1];
	}
	memcpy(&record->menu, v->cm, sizeof(record->menu));
	memcpy(&record->info, v->dspinfo, sizeof(record->info));
}

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
			{
				int result = ops->process(dp[ep], in[ep], out[ep], FRAG);
				capture_v8(&v8_oracle[ep][blk], dp[ep], ep, 0, result, out[ep]);
				v8_active[ep][blk] = 1;
				v8_calls[ep]++;
				if (result != DPSTAT_OK)
					done[ep] = 1;
			}
		}
		wire_put(out);
	}
	v8_ok = done[0] && done[1];
	memcpy(v8_rt_before_destroy, rt_buf, sizeof(rt_buf));
	memcpy(v8_info_before_destroy, info_buf, sizeof(info_buf));

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

/* Fixed public-output oracle, captured only from the original blob/blob run.
 * Samples are compared directly; no digest stands in for sample equality. */
struct lifecycle_block {
	short samples[FRAG];
	int result, progress, mode;
};
static struct lifecycle_block lifecycle_oracle[NEP][V34_BLOCKS];

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

/* The second pass partitions each host block without changing the wire.
 * Every run has its own construction, runtime snapshot and writable samples.
 * No queue, modem or handshake state is planted. */
static int queue_chunks;
static int queue_pending[4], queue_no_process, queue_muted, queue_active;
struct queue_observation {
	short output[FRAG], pending[3], tail[4];
	int status, in_count, out_count, progress, mode, nbits, mute;
};
static struct queue_observation queue_oracle[NEP][V34_BLOCKS][4];

static int
process_chunks(struct dp *dp, short *input, short *output,
               int run, int ep, int blk)
{
	static const int chunks[] = { 1, 2, 3, 42 };
	struct vpcm_root *s = (struct vpcm_root *)dp->dp_data;
	int k, off = 0, status = DPSTAT_OK;
	for (k = 0; k < 4; k++) {
		struct queue_observation obs;
		int count = chunks[k], pending = s->inq.count;
		int nproc = ((pending + count) / 4) * 4;
		int muted = s->mute > 0;
		int rc;
		long tag = ((run * NEP + ep) * V34_BLOCKS + blk) * 4 + k;
		if ((run >> ep) & 1)
			rc = g_our34->process(dp, input + off, output + off, count);
		else
			rc = ref_vpcm_run(dp, input + off, output + off, count);
		if (rc != DPSTAT_OK)
			status = rc;
		queue_pending[pending]++;
		if (!nproc) queue_no_process++;
		else if (muted) queue_muted++;
		else queue_active++;
		diff_eq_int("chunk input occupancy (%ld)", s->inq.count,
			    pending + count - nproc, tag);
		diff_eq_int("chunk output occupancy (%ld)", s->outq.count,
			    4 - s->inq.count, tag);
		memset(&obs, 0, sizeof(obs));
		memcpy(obs.output, output + off, count * sizeof(short));
		memcpy(obs.pending, s->inq.buf, s->inq.count * sizeof(short));
		memcpy(obs.tail, s->outq.buf, s->outq.count * sizeof(short));
		obs.status = rc; obs.in_count = s->inq.count;
		obs.out_count = s->outq.count; obs.progress = s->status;
		obs.mode = s->mode; obs.nbits = s->nbits; obs.mute = s->mute;
		if (run == 0)
			queue_oracle[ep][blk][k] = obs;
		else diff_eq_obj("chunk status/samples/queues", struct queue_observation,
		                &obs, &queue_oracle[ep][blk][k], tag);
		off += count;
	}
	return status;
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
			if (queue_chunks)
				rc = process_chunks(dp[ep], in[ep], out[ep],
		              run, ep, blk);
			else if ((run >> ep) & 1)
				rc = g_our34->process(dp[ep], in[ep], out[ep],
						      FRAG);
			else
				rc = ref_vpcm_run(dp[ep], in[ep], out[ep],
						  FRAG);
			alarm(0);

			blkdig[run][ep][blk] = digest(rc, out[ep], FRAG);
			if (!queue_chunks && run == 0) {
				struct lifecycle_block *b = &lifecycle_oracle[ep][blk];
				memcpy(b->samples, out[ep], sizeof(b->samples));
				b->result = rc;
				b->progress = ((struct vpcm_root *)root[ep])->status;
				b->mode = ((struct vpcm_root *)root[ep])->mode;
			}

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

static int
run_queue_calls(struct dp_operations *ops)
{
	typedef unsigned char bit_transcript[HARNESS_SHIM_BITS];
	static bit_transcript oracle_bits[NEP];
	int run, ep, rc, captured_bits = 0;
	diff_begin("V.PCM chunked connecting lifecycle: 1/2/3/42 samples");
	queue_chunks = 1;
	for (run = 0; run < NRUN; run++) {
		run_v34(ops, run);
		for (ep = 0; ep < NEP; ep++) {
			const struct v34res *r = &v34res[run][ep];
			struct modem_shim *shim = shim_for_ep(run, ep);
			captured_bits += r->rx_len;
			if (run == 0)
				memcpy(oracle_bits[ep], shim->rx, sizeof(bit_transcript));
			else diff_eq_obj("chunk received bit transcript", bit_transcript,
		                 shim->rx, oracle_bits[ep], run*2+ep);
			diff_eq_int("chunk call constructed (%ld)", r->built, 1, run*2+ep);
			diff_eq_int("chunk call connected (%ld)", r->mode,
			    MODE_CONNECTED, run*2+ep);
			diff_eq_int("chunk call connect once (%ld)", r->nconnect, 1, run*2+ep);
			diff_eq_int("chunk call no link error (%ld)", r->nerror, 0, run*2+ep);
			diff_eq_int("chunk payload observer live (%ld)", r->aligned > 1000,
			    1, run*2+ep);
			diff_eq_int("chunk payload BER (%ld)", r->errors, 0, run*2+ep);
			diff_eq_int("chunk payload length (%ld)", r->rx_len,
			    v34res[0][ep].rx_len, run*2+ep);
			diff_eq_int("chunk payload alignment (%ld)", r->lag,
			    v34res[0][ep].lag, run*2+ep);
			diff_eq_int("chunk bit fetch count (%ld)", r->gets,
			    v34res[0][ep].gets, run*2+ep);
			diff_eq_int("chunk bit delivery count (%ld)", r->puts,
			    v34res[0][ep].puts, run*2+ep);
		}
	}
	queue_chunks = 0;
	printf("  chunk lifecycle: %d runs x %d endpoints x %d blocks x 4 calls; "
	       "pending 0/1/2/3 = %d/%d/%d/%d; "
	       "no-process/muted/active = %d/%d/%d; captured bits = %d\n",
	       NRUN, NEP, V34_BLOCKS, queue_pending[0], queue_pending[1],
	       queue_pending[2], queue_pending[3], queue_no_process,
	       queue_muted, queue_active, captured_bits);
	for (ep = 0; ep < 4; ep++)
		diff_eq_int("pending occupancy exercised (%ld)", queue_pending[ep] > 0,
			    1, ep);
	diff_eq_int("zero-process calls exercised", queue_no_process > 0, 1, 0);
	diff_eq_int("muted queue calls exercised", queue_muted > 0, 1, 0);
	diff_eq_int("active queue calls exercised", queue_active > 0, 1, 0);
	rc = diff_end();
	return rc;
}

/* Matched ownership at the public V.PCM boundary.  No callbacks, queues or
 * modem states are planted.  The borrowed V.8 snapshot, channel and horizon
 * are exactly those of the original processing cohort. */
extern void scrambleGPA(void), scrambleGPC(void);
extern void descrambleGPA(void), descrambleGPC(void);
extern void ref_scrambleGPA(void), ref_scrambleGPC(void);
extern void ref_descrambleGPA(void), ref_descrambleGPC(void);

static int
lifecycle_callbacks(struct vpcm_root *s, int ours, int ep, long tag)
{
	/* v34handshakinit's role-selected code pointers, independently named in
	 * t_vpcmcreate.  Different image addresses have the same canonical role. */
	const void *tx[2][NEP] = {
		{ ref_scrambleGPC, ref_scrambleGPA },
		{ scrambleGPC, scrambleGPA }
	};
	const void *rx[2][NEP] = {
		{ ref_descrambleGPA, ref_descrambleGPC },
		{ descrambleGPA, descrambleGPC }
	};
	void *actual_tx, *actual_rx;
	int before = diff_failures;
	memcpy(&actual_tx, (char *)&s->v34 + 0x2a28, sizeof(actual_tx));
	memcpy(&actual_rx, (char *)&s->v34 + 0x0e48, sizeof(actual_rx));
	diff_eq_int("lifecycle transmit callback role/image", actual_tx == tx[ours][ep], 1, tag);
	diff_eq_int("lifecycle receive callback role/image", actual_rx == rx[ours][ep], 1, tag);
	return diff_failures == before;
}

/* Observe real V.34 receiver histories without changing the channel recipe.
 * Selector 0 drains its ring: both images use the same checkpoint schedule.
 * This cohort covers status 0 only; PCM and K56 dispatch remain uncovered. */
extern unsigned long ref_VPcmV34GetVisualDiagnostics(void *obj, int what,
				struct int_complex *points, unsigned long maxCount);

#define VISUAL_POINTS 8
#define VISUAL_SELECTORS 6
#define VISUAL_CHECKPOINTS 5

struct visual_checkpoint {
	int block;
	unsigned long capacity;
};

static const struct visual_checkpoint visual_schedule[VISUAL_CHECKPOINTS] = {
	{ 1000, 1 }, { 1500, 2 }, { 1600, 4 }, { 2500, 8 }, { 3999, 8 }
};
static const int visual_selectors[VISUAL_SELECTORS] = { 0, 1, 3, 4, 5, 6 };

struct visual_result {
	unsigned guard[2];
	struct int_complex points[VISUAL_POINTS + 2];
	unsigned long count;
};

static struct visual_result visual_native[NEP][VISUAL_CHECKPOINTS][VISUAL_SELECTORS];
static int visual_queries, visual_nonempty[VISUAL_SELECTORS];
static int visual_nonzero[VISUAL_SELECTORS];
static int visual_distinct_echo_queries, visual_distinct_echo_witnesses;
static int visual_fault_fired;

/* Native-address known answers are separate from the two-image comparison.
 * Explicit widths matter: phase is int; offset and coefficient values are short.
 * Pointer identities below establish storage ownership, not unequal values. */
static void
visual_known_answer(struct v34_object *v, int selector, unsigned long capacity,
		    struct visual_result *expected, long tag)
{
	unsigned long n = 0, i;

	if (selector == 0) {
		n = (unsigned int)v->hist1_idx;
		if (n > capacity)
			n = capacity;
		for (i = 0; i < n; i++) {
			short raw[2];
			memcpy(raw, (char *)v + 0x2aa8 + 4 * i, sizeof(raw));
			expected->points[i].re = 2 * (int)raw[0];
			expected->points[i].im = 2 * (int)raw[1];
		}
	} else if (selector == 1) {
		n = capacity; /* Every scheduled capacity is below the native cap 80. */
		for (i = 0; i < n; i++) {
			short re, im;
			memcpy(&re, (char *)v + 0x264 + 0x50c + 2 * i, sizeof(re));
			memcpy(&im, (char *)v + 0x264 + 0x5ac + 2 * i, sizeof(im));
			diff_eq_int("visual raw EQ real provenance", re, v->receiver.equalizer.re[i], tag);
			diff_eq_int("visual raw EQ imag provenance", im, v->receiver.equalizer.im[i], tag);
			expected->points[i].re = 2 * (int)re;
			expected->points[i].im = 2 * (int)im;
		}
	} else if (selector == 3 || selector == 4) {
		int phase;
		short offset;
		n = 1; /* Native selectors 3/4 always write one point, even at cap 0. */
		memcpy(&phase, (char *)v + 0x43c, sizeof(phase));
		memcpy(&offset, (char *)v + 0x434, sizeof(offset));
		diff_eq_int("visual raw phase provenance", phase, v->receiver.timing_frac, tag);
		diff_eq_int("visual raw offset provenance", offset, v->receiver.timing_ppm, tag);
		expected->points[0].re = selector == 3 ? phase : offset;
		expected->points[0].im = 0;
	} else {
		struct v34_echo *echo = selector == 5 ? &v->echo0 : &v->echo1;
		unsigned owner_offset = selector == 5 ? 0x80b8 : 0x9138;
		short *raw_coeff;
		unsigned raw_taps;
		memcpy(&raw_coeff, (char *)v + owner_offset + 8, sizeof(raw_coeff));
		memcpy(&raw_taps, (char *)v + owner_offset + 0x1c, sizeof(raw_taps));
		diff_eq_int("visual raw echo coefficient owner", raw_coeff == echo->coeff, 1, tag);
		diff_eq_int("visual inline echo coefficient storage",
			    echo->coeff == (selector == 5 ? v->echo0_coeff : v->echo1_coeff), 1, tag);
		diff_eq_int("visual raw echo tap count provenance", raw_taps, echo->taps, tag);
		n = raw_taps;
		if (n > capacity)
			n = capacity;
		for (i = 0; i < n; i++) {
			expected->points[i].re = 0;
			expected->points[i].im = raw_coeff[i];
		}
	}
	expected->count = n;
}

static int
visual_after_block(struct vpcm_root *rootp, int run, int ep, int block)
{
	struct v34_object *v = (struct v34_object *)&rootp->v34;
	static struct vpcm_root root_before, root_expected;
	int checkpoint, k, before = diff_failures;

	for (checkpoint = 0; checkpoint < VISUAL_CHECKPOINTS; checkpoint++)
		if (visual_schedule[checkpoint].block == block)
			break;
	if (checkpoint == VISUAL_CHECKPOINTS)
		return 1;
	for (k = 0; k < VISUAL_SELECTORS; k++) {
		struct visual_result observed, expected;
		unsigned long capacity = visual_schedule[checkpoint].capacity, i;
		int selector = visual_selectors[k], nonzero = 0, distinct_echo = 0;
		int raw_status;
		short raw_cursor;
		long tag = ((run * NEP + ep) * VISUAL_CHECKPOINTS + checkpoint) * 10 + selector;

		memset(&observed, 0xa5, sizeof(observed));
		memcpy(&expected, &observed, sizeof(expected));
		memcpy(&raw_status, v, sizeof(raw_status));
		memcpy(&raw_cursor, (char *)v + 0x2aa4, sizeof(raw_cursor));
		diff_eq_int("visual live status 0", v->status, 0, tag);
		diff_eq_int("visual live role", v->role, ep_role[ep], tag);
		diff_eq_int("visual public root owner", rootp->dp.dp_data == rootp, 1, tag);
		diff_eq_int("visual raw status owner+0", raw_status, v->status, tag);
		diff_eq_int("visual raw signed history cursor", raw_cursor, v->hist1_idx, tag);
		if (diff_failures != before)
			return 0;
		visual_known_answer(v, selector, capacity, &expected, tag);
		if (diff_failures != before)
			return 0;
		/* Include padding in the root-byte mutation observation. External
		 * allocations are outside this claim; public behavior is checked too. */
		memcpy(&root_before, rootp, sizeof(root_before));
		memcpy(&root_expected, &root_before, sizeof(root_expected));
		if (selector == 0) {
			short zero = 0;
			memcpy((char *)&root_expected.v34 + 0x2aa4, &zero, sizeof(zero));
		}
		if (run == 0)
			observed.count = ref_VPcmV34GetVisualDiagnostics(v, selector, observed.points, capacity);
		else
			observed.count = VPcmV34GetVisualDiagnostics(v, selector, observed.points, capacity);
		visual_queries++;
		/* Change a copied diagnostic only; the modem and wire stay intact. */
		if (getenv("VPCM_VISUAL_COPY_FAULT") && run == 3 && checkpoint == 2 && ep == 0 && selector == 1 && observed.count) {
			observed.points[0].re ^= 1;
			visual_fault_fired++;
		}
		diff_eq_obj("visual raw-field known answer/full guarded buffer", struct visual_result,
			    &observed, &expected, tag);
		diff_eq_obj("visual only permitted root-byte mutation", struct vpcm_root,
			    rootp, &root_expected, tag);
		if (run == 0)
			memcpy(&visual_native[ep][checkpoint][k], &observed, sizeof(observed));
		else
			diff_eq_obj("visual exact native count/full guarded buffer", struct visual_result,
				    &observed, &visual_native[ep][checkpoint][k], tag);
		if (diff_failures != before)
			return 0;
		if (observed.count)
			visual_nonempty[k]++;
		for (i = 0; i < observed.count; i++)
			if (observed.points[i].re || observed.points[i].im)
				nonzero++;
		if (nonzero)
			visual_nonzero[k]++;
		if (selector == 5 || selector == 6) {
			for (i = 0; i < observed.count && i < v->echo0.taps && i < v->echo1.taps; i++)
				if (v->echo0.coeff[i] != v->echo1.coeff[i])
					distinct_echo++;
			if (distinct_echo) {
				visual_distinct_echo_queries++;
				if (selector == 5)
					visual_distinct_echo_witnesses++;
			}
		}
		printf("  visual %s %s block %d status %d role %#x selector %d capacity %lu count %lu nonzero %d history %d near/far %u/%u distinct %d first %d/%d\n",
		       run_name[run], ep_name[ep], block, v->status, v->role, selector, capacity,
		       observed.count, nonzero, raw_cursor, v->echo0.taps, v->echo1.taps, distinct_echo,
		       observed.count ? observed.points[0].re : 0, observed.count ? observed.points[0].im : 0);
	}
	return 1;
}

static void
visual_finish(void)
{
	int k;
	diff_eq_int("visual query denominator", visual_queries, 2 * NEP * VISUAL_CHECKPOINTS * VISUAL_SELECTORS, 0);
	for (k = 0; k < VISUAL_SELECTORS; k++) {
		diff_eq_int("visual nonempty queries per selector", visual_nonempty[k], 2 * NEP * VISUAL_CHECKPOINTS, k);
		diff_eq_int("visual nonzero adapted observations per selector", visual_nonzero[k] > 0, 1, k);
	}
	diff_eq_int("visual naturally distinct near/far value witnesses", visual_distinct_echo_witnesses > 0, 1, 0);
	printf("  visual: %d/120 queries; nonempty %d/%d/%d/%d/%d/%d; nonzero %d/%d/%d/%d/%d/%d; distinct near/far %d/20 value witnesses, %d selector observations; copied faults %d\n",
	       visual_queries, visual_nonempty[0], visual_nonempty[1], visual_nonempty[2],
	       visual_nonempty[3], visual_nonempty[4], visual_nonempty[5], visual_nonzero[0],
	       visual_nonzero[1], visual_nonzero[2], visual_nonzero[3], visual_nonzero[4],
	       visual_nonzero[5], visual_distinct_echo_witnesses, visual_distinct_echo_queries,
	       visual_fault_fired);
}

/* This is a sequential API chain under the existing phase-boundary resets,
 * not a continuous-wire replacement scheduler or telephone interoperability.
 * Connection alone is insufficient: V.34 does not consume every negotiated
 * menu byte. Publication and producer/consumer ownership have separate checks. */
static int handoff_v8_created, handoff_v8_destroyed, handoff_v8_calls;
static int handoff_v8_completed, handoff_admissions, handoff_copy_rejected;

static int
handoff_v8_ownership(struct v8_dp *v, struct dp_operations *owner, int ep, long tag)
{
	int before = diff_failures;
	diff_eq_int("handoff V8 self owner", v->self == v, 1, tag);
	diff_eq_int("handoff V8 operations owner", v->op == owner, 1, tag);
	diff_eq_int("handoff V8 modem owner", v->modem == ep_modem[ep], 1, tag);
	diff_eq_int("handoff V8 requested constructor id", v->want, DP_V34, tag);
	diff_eq_int("handoff V8 registered id", v->id, DP_V8, tag);
	diff_eq_int("handoff V8 runtime/menu owner", (void *)v->cm == (void *)rt_buf[ep], 1, tag);
	diff_eq_int("handoff V8 DSPINFO owner", (void *)v->dspinfo == (void *)info_buf[ep], 1, tag);
	diff_eq_int("handoff V8 handshake present", v->v8 != 0, 1, tag);
	if (v->v8)
		diff_eq_int("handoff V8 handshake menu owner", v->v8->cm == v->cm, 1, tag);
	return diff_failures == before;
}

static int
source_v8_handoff(int trial, int request[NEP])
{
	struct dp *dp[NEP] = { 0, 0 };
	int done[NEP] = { 0, 0 }, calls[NEP] = { 0, 0 };
	int stop = 0, ep, block, before = diff_failures;
	int baseline_live = harness_alloc.live, baseline_bad = harness_alloc.bad_free;
	int baseline_balance = harness_alloc.allocs - harness_alloc.frees;
	short in[NEP][FRAG], out[NEP][FRAG];

	routes_reset();
	for (ep = 0; ep < NEP; ep++) {
		long tag = trial * NEP + ep;
		memset(rt_buf[ep], 0, sizeof(rt_buf[ep]));
		memset(info_buf[ep], 0, sizeof(info_buf[ep]));
		request[ep] = -1;
		params_for(ep);
		/* The registered V8 table is id 8; its create argument asks for 34. */
		dp[ep] = g_our8->create(ep_modem[ep], DP_V34, ep_caller[ep], SRATE, FRAG, g_our8);
		diff_eq_int("handoff source V8 created", dp[ep] != 0, 1, tag);
		if (!dp[ep]) { stop = 1; break; }
		handoff_v8_created++;
		diff_eq_int("handoff V8 wrapper allocation admitted", harness_alloc_ordinal(dp[ep]) != 0, 1, tag);
		if (diff_failures != before) { stop = 1; break; }
		diff_eq_int("handoff V8 constructor handshake present", ((struct v8_dp *)dp[ep])->v8 != 0, 1, tag);
		if (((struct v8_dp *)dp[ep])->v8)
			diff_eq_int("handoff V8 handshake allocation admitted", harness_alloc_ordinal(((struct v8_dp *)dp[ep])->v8) != 0, 1, tag);
		if (diff_failures != before) { stop = 1; break; }
		if (!handoff_v8_ownership((struct v8_dp *)dp[ep], g_our8, ep, tag)) {
			stop = 1;
			break;
		}
	}
	wire_reset();
	for (block = 0; block < V8_MAX_BLOCKS && !(done[0] && done[1]) && !stop; block++) {
		cur_run = 8 + trial;
		cur_block = block;
		wire_take(in);
		for (ep = 0; ep < NEP; ep++) {
			struct v8_block observed;
			int result;
			long tag = (trial * NEP + ep) * V8_MAX_BLOCKS + block;
			memset(out[ep], 0, sizeof(out[ep]));
			if (done[ep])
				continue;
			cur_ep = ep;
			alarm(30);
			result = g_our8->process(dp[ep], in[ep], out[ep], FRAG);
			alarm(0);
			calls[ep]++;
			handoff_v8_calls++;
			capture_v8(&observed, dp[ep], ep, 1, result, out[ep]);
			diff_eq_int("handoff native V8 block was active", v8_active[ep][block], 1, tag);
			diff_eq_obj("handoff V8 exact samples/return/publication", struct v8_block,
				    &observed, &v8_oracle[ep][block], tag);
			if (!handoff_v8_ownership((struct v8_dp *)dp[ep], g_our8, ep, tag) || diff_failures != before) {
				stop = 1;
				break;
			}
			if (result != DPSTAT_OK) {
				diff_eq_int("handoff V8 transition result", result, DPSTAT_CHANGEDP, tag);
				diff_eq_int("handoff V8 transition request count", observed.nparams, 1, tag);
				diff_eq_int("handoff V8 transition parameter", observed.parameter, MDMPRM_DP_REQUESTED, tag);
				diff_eq_int("handoff V8 transition request", observed.request, DP_V34, tag);
				diff_eq_int("handoff V8 other image callback unused", observed.other_params, 0, tag);
				diff_eq_int("handoff V8 published LAPM", observed.info.qc_lapm, (observed.menu.b2 >> 6) & 1, tag);
				diff_eq_int("handoff V8 published index", observed.info.qc_index, observed.menu.menu, tag);
				if (diff_failures != before) { stop = 1; break; }
				request[ep] = observed.request;
				done[ep] = 1;
				printf("  handoff V8 trial %d %s block %d: result %d request %d menu %02x/%02x/%02x QC %u/%u\n",
				       trial, ep_name[ep], block, result, request[ep], observed.menu.b0,
				       observed.menu.b1, observed.menu.b2, observed.info.qc_lapm, observed.info.qc_index);
			}
		}
		if (!stop)
			wire_put(out);
	}
	if (!stop) {
		for (ep = 0; ep < NEP; ep++) {
			diff_eq_int("handoff V8 endpoint completed within horizon", done[ep], 1, trial * NEP + ep);
			diff_eq_int("handoff V8 active call denominator", calls[ep], v8_calls[ep], trial * NEP + ep);
		}
		diff_eq_obj("handoff full runtime before V8 destroy", long[NEP][CFG_BUFWORDS], rt_buf, v8_rt_before_destroy, trial);
		diff_eq_obj("handoff full info before V8 destroy", long[NEP][CFG_BUFWORDS], info_buf, v8_info_before_destroy, trial);
		if (diff_failures != before)
			stop = 1;
	}
	for (ep = 0; ep < NEP; ep++) if (dp[ep]) {
		diff_eq_int("handoff owning V8 destructor", g_our8->destroy(dp[ep]), 0, trial * NEP + ep);
		handoff_v8_destroyed++;
	}
	diff_eq_int("handoff V8 allocations restored", harness_alloc.live, baseline_live, trial);
	diff_eq_int("handoff V8 allocation/free balance", harness_alloc.allocs - harness_alloc.frees, baseline_balance, trial);
	diff_eq_int("handoff V8 no invalid destruction", harness_alloc.bad_free, baseline_bad, trial);
	diff_eq_int("handoff V8 allocation accounting valid", harness_alloc.overflow, 0, trial);
	if (!stop && diff_failures == before) {
		int observed_request[NEP];
		diff_eq_obj("handoff full runtime after V8 destroy", long[NEP][CFG_BUFWORDS], rt_buf, rt_snap, trial);
		diff_eq_obj("handoff full info after V8 destroy", long[NEP][CFG_BUFWORDS], info_buf, info_snap, trial);
		if (diff_failures == before)
			handoff_v8_completed++;
		memcpy(observed_request, request, sizeof(observed_request));
		/* Reject a copied observation before any new DP is created. The
		 * actual callback records, runtime, info and wire remain unchanged. */
		if (getenv("VPCM_HANDOFF_COPY_FAULT") && trial == 0)
			observed_request[0] = 23;
		for (ep = 0; ep < NEP; ep++)
			diff_eq_int("handoff copied request admission", observed_request[ep], DP_V34, trial * NEP + ep);
		if (diff_failures != before && getenv("VPCM_HANDOFF_COPY_FAULT")) {
			handoff_copy_rejected++;
			diff_eq_int("handoff rejects before downstream create", handoff_admissions, 0, trial);
		}
	}
	if (diff_failures != before)
		stop = 1;
	return !stop;
}

static int
handoff_control(struct dp_operations *reference)
{
	int ep, k, rc;
	diff_begin("V8 handoff reference control and source driver admission");
	diff_eq_int("handoff source V8 operations registered", g_our8 != 0, 1, 0);
	if (!g_our8)
		return diff_end();
	diff_eq_int("handoff source V8 registered identity", our_ops_for(DP_V8) == g_our8, 1, 0);
	diff_eq_int("handoff source V8 table name", g_our8->name && strcmp(g_our8->name, "V8") == 0, 1, 0);
	diff_eq_int("handoff source V8 entries present", g_our8->create && g_our8->process && g_our8->destroy, 1, 0);
	diff_eq_int("handoff source V8 create image", g_our8->create != reference->create, 1, 0);
	diff_eq_int("handoff source V8 process image", g_our8->process != reference->process, 1, 0);
	diff_eq_int("handoff source V8 destroy image", g_our8->destroy != reference->destroy, 1, 0);
	for (ep = 0; ep < NEP; ep++) {
		const struct v8_block *last;
		diff_eq_int("handoff reference V8 calls nonempty", v8_calls[ep] > 0, 1, ep);
		diff_eq_int("handoff reference V8 within horizon", v8_calls[ep] <= V8_MAX_BLOCKS, 1, ep);
		if (!v8_calls[ep] || v8_calls[ep] > V8_MAX_BLOCKS)
			continue;
		last = &v8_oracle[ep][v8_calls[ep] - 1];
		diff_eq_int("handoff reference transition result", last->result, DPSTAT_CHANGEDP, ep);
		diff_eq_int("handoff reference transition request", last->request, DP_V34, ep);
		diff_eq_int("handoff reference request parameter", last->parameter, MDMPRM_DP_REQUESTED, ep);
		diff_eq_int("handoff reference exactly one callback", last->nparams, 1, ep);
		diff_eq_int("handoff reference other image unused", last->other_params, 0, ep);
		diff_eq_int("handoff reference menu b0", last->menu.b0, 0xa1, ep);
		diff_eq_int("handoff reference menu b1", last->menu.b1, 0x40, ep);
		diff_eq_int("handoff reference menu b2", last->menu.b2, 0, ep);
		for (k = 0; k < v8_calls[ep] - 1; k++)
			diff_eq_int("handoff reference active prefix return", v8_oracle[ep][k].result, DPSTAT_OK, ep * V8_MAX_BLOCKS + k);
	}
	rc = diff_end();
	return rc;
}

struct lifecycle_plan {
	const char *name;
	int cases;
	int run[3];
	int source_owners;
	int observe_visual;
	int source_v8;
};

static const struct lifecycle_plan unobserved_lifecycles = {
	"V.PCM matched source-constructor connecting lifecycles", 3, { 1, 2, 3 }, 4, 0, 0
};
static const struct lifecycle_plan observed_lifecycles = {
	"V.PCM visual diagnostics on an adapted connecting lifecycle", 2, { 0, 3 }, 2, 1, 0
};

static const struct lifecycle_plan handoff_lifecycles = {
	"source V8 publication through a sequential V34 API lifecycle", 2, { 0, 3 }, 2, 0, 1
};

static int
run_lifecycles(struct dp_operations *reference, const struct lifecycle_plan *plan)
{
	int trial, run, ep, blk, calls = 0, destroyed = 0, bits[NEP] = { 0, 0 };
	int completed = 0, created = 0, source_created = 0, reference_created = 0;
	int copied_control = !plan->observe_visual && !plan->source_v8 && getenv("VPCMRUN_LIFECYCLE_COPY_FAULT") != 0;
	int stop = 0;
	void (*prev)(int) = signal(SIGALRM, call_alarm);

	diff_begin(plan->name);
	diff_eq_int("lifecycle reference create identity", reference->create == ref_vpcm_create, 1, 0);
	diff_eq_int("lifecycle reference table identity", reference == &ref_vpcm_op, 1, 0);
	/* Source entries are file-static. Their canonical identity is the
	 * table registered by dp_vpcm_init, not a fabricated exported alias. */
	diff_eq_int("lifecycle registered source owner", our_ops_for(34) == g_our34, 1, 0);
	diff_eq_int("lifecycle source table present", g_our34->create && g_our34->process && g_our34->destroy, 1, 0);
	diff_eq_int("lifecycle process images differ", g_our34->process != reference->process, 1, 0);
	diff_eq_int("lifecycle create images differ", g_our34->create != reference->create, 1, 0);
	diff_eq_int("lifecycle destroy images differ", g_our34->destroy != reference->destroy, 1, 0);
	if (diff_failures) stop = 1;
	for (trial = 0; trial < plan->cases && !stop; trial++) {
		struct dp *dp[NEP] = { 0, 0 };
		struct dp_operations *owner[NEP];
		int connected[NEP] = { -1, -1 }, nconnect[NEP] = { 0, 0 };
		int baseline_live = harness_alloc.live, baseline_bad = harness_alloc.bad_free;
		int baseline_balance = harness_alloc.allocs - harness_alloc.frees;
		int before = diff_failures;
		short in[NEP][FRAG], out[NEP][FRAG];
		int request[NEP] = { DP_V34, DP_V34 };

		run = plan->run[trial];
		cur_run = NRUN + run;
		if (plan->source_v8) {
			if (!source_v8_handoff(trial, request)) { stop = 1; break; }
		} else {
			memcpy(rt_buf, rt_snap, sizeof(rt_buf));
			memcpy(info_buf, info_snap, sizeof(info_buf));
		}
		/* Producer processing uses its own alarm tag; restore the consumer. */
		cur_run = NRUN + run;
		/* Existing phase boundary: reset the wire and per-phase route log. */
		routes_reset();
		for (ep = 0; ep < NEP; ep++) {
			int ours = (run >> ep) & 1;
			long tag = run * NEP + ep;
			struct vpcm_root *s;
			owner[ep] = ours ? g_our34 : reference;
			if (plan->source_v8) {
				owner[ep] = ours ? our_ops_for(request[ep]) : ops_for(request[ep]);
				diff_eq_int("handoff requested consumer operations found", owner[ep] != 0, 1, tag);
				diff_eq_int("handoff requested consumer identity", owner[ep] == (ours ? g_our34 : reference), 1, tag);
				if (diff_failures != before) { stop = 1; break; }
				printf("  handoff admission trial %d %s request %d owner %s/%s\n",
				       trial, ep_name[ep], request[ep], ours ? "source" : "reference", owner[ep]->name);
			}
			params_for(ep);
			if (plan->source_v8) handoff_admissions++;
			dp[ep] = owner[ep]->create(ep_modem[ep], request[ep], ep_caller[ep], SRATE, FRAG, owner[ep]);
			diff_eq_int("lifecycle allocation", dp[ep] != 0, 1, tag);
			if (!dp[ep]) { stop = 1; break; }
			created++;
			if (ours) source_created++; else reference_created++;
			s = (struct vpcm_root *)dp[ep];
			diff_eq_int("lifecycle operations owner", dp[ep]->op == owner[ep], 1, tag);
			diff_eq_int("lifecycle root ownership", dp[ep]->dp_data == dp[ep], 1, tag);
			diff_eq_int("lifecycle modem ownership", dp[ep]->modem == ep_modem[ep], 1, tag);
			diff_eq_int("lifecycle runtime ownership", (void *)s->params == (void *)rt_buf[ep], 1, tag);
			diff_eq_int("lifecycle info ownership", (void *)s->info == (void *)info_buf[ep], 1, tag);
			diff_eq_int("lifecycle graph allocated", harness_alloc_ordinal(dp[ep]) != 0, 1, tag);
			if (!lifecycle_callbacks(s, ours, ep, tag) || diff_failures != before) {
				stop = 1; break;
			}
		}
		wire_reset();
		for (blk = 0; blk < V34_BLOCKS && !stop; blk++) {
			cur_block = blk;
			wire_take(in);
			for (ep = 0; ep < NEP; ep++) {
				struct vpcm_root *s = (struct vpcm_root *)dp[ep];
				struct lifecycle_block observed;
				long tag = (run * NEP + ep) * V34_BLOCKS + blk;
				cur_ep = ep;
				memset(out[ep], 0, sizeof(out[ep]));
				memset(&observed, 0, sizeof(observed));
				alarm(30);
				observed.result = owner[ep]->process(dp[ep], in[ep], out[ep], FRAG);
				alarm(0);
				calls++;
				memcpy(observed.samples, out[ep], sizeof(observed.samples));
				observed.progress = s->status;
				observed.mode = s->mode;
				/* Negative calibration changes a copied observation only. */
				if (copied_control && run == 1 && ep == 0 && blk == 1600)
					observed.samples[0] ^= 1;
				diff_eq_obj("lifecycle exact samples/return/status", struct lifecycle_block,
				            &observed, &lifecycle_oracle[ep][blk], tag);
				if (diff_failures != before) { stop = 1; break; }
				if (plan->observe_visual && !visual_after_block(s, run, ep, blk)) {
					stop = 1;
					break;
				}
				if (observed.result == DPSTAT_CONNECT) {
					connected[ep] = blk;
					nconnect[ep]++;
				}
			}
			if (!stop) wire_put(out);
		}
		if (!stop) for (ep = 0; ep < NEP; ep++) {
			struct modem_shim *s = shim_for_ep(run, ep);
			struct modem_shim *unused = ((run >> ep) & 1)
				? &harness_modem_route_ref[ep_route[ep]]
				: &harness_modem_route_ours[ep_route[ep]];
			int lag, counted, errors;
			long tag = run * NEP + ep;
			root[ep] = (char *)dp[ep]; obj[ep] = root[ep] + ROOT_V34;
			diff_eq_int("lifecycle connect block", connected[ep], v34res[0][ep].connect_rc_blk, tag);
			diff_eq_int("lifecycle connect event count", nconnect[ep], 1, tag);
			diff_eq_int("lifecycle transmit rate", peek16(ep, O_TXRATE), RATE_33600, tag);
			diff_eq_int("lifecycle receive rate", peek16(ep, O_RXRATE), RATE_33600, tag);
			diff_eq_int("lifecycle bit source active", s->gets > 0, 1, tag);
			diff_eq_int("lifecycle bit sink active", s->puts > 0, 1, tag);
			diff_eq_int("lifecycle other image bit source unused", unused->gets, 0, tag);
			diff_eq_int("lifecycle other image bit sink unused", unused->puts, 0, tag);
			errors = align_errors(s->rx, s->rx_len, pattern[ep ^ 1], PAT_LEN, &lag, &counted);
			diff_eq_int("lifecycle independently aligned payload", counted >= 2048, 1, tag);
			diff_eq_int("lifecycle payload BER zero", errors, 0, tag);
			bits[ep] += counted;
			printf("  lifecycle %s %s: connect %d; payload %d bits, errors %d\n",
			       run_name[run], ep_name[ep], connected[ep], counted, errors);
		}
		for (ep = 0; ep < NEP; ep++) if (dp[ep]) {
			diff_eq_int("lifecycle owning destructor result", owner[ep]->destroy(dp[ep]), 0, run * NEP + ep);
			destroyed++;
		}
		diff_eq_int("lifecycle allocations restored", harness_alloc.live, baseline_live, run);
		diff_eq_int("lifecycle allocation/free balance", harness_alloc.allocs - harness_alloc.frees, baseline_balance, run);
		diff_eq_int("lifecycle no invalid destruction", harness_alloc.bad_free, baseline_bad, run);
		diff_eq_int("lifecycle allocation accounting valid", harness_alloc.overflow, 0, run);
		if (diff_failures != before) stop = 1;
		else completed++;
	}
	diff_eq_int("lifecycle completed cases", completed, plan->cases, 0);
	diff_eq_int("lifecycle created endpoint owners", created, plan->cases * NEP, 0);
	diff_eq_int("lifecycle source endpoint owners", source_created, plan->source_owners, 0);
	diff_eq_int("lifecycle reference endpoint owners", reference_created, plan->cases * NEP - plan->source_owners, 0);
	diff_eq_int("lifecycle owning destructors executed", destroyed, plan->cases * NEP, 0);
	diff_eq_int("lifecycle endpoint calls executed", calls, plan->cases * NEP * V34_BLOCKS, 0);
	diff_eq_int("lifecycle output sample positions compared", calls * FRAG, plan->cases * NEP * V34_BLOCKS * FRAG, 0);
	for (ep = 0; ep < NEP; ep++)
		diff_eq_int("lifecycle aggregate aligned directional payload", bits[ep] >= plan->cases * 2048, 1, ep);
	if (plan->observe_visual)
		visual_finish();
	if (plan->source_v8) {
		diff_eq_int("handoff source V8 pairs completed", handoff_v8_completed, 2, 0);
		diff_eq_int("handoff V8 owners created", handoff_v8_created, 4, 0);
		diff_eq_int("handoff V8 owners destroyed", handoff_v8_destroyed, 4, 0);
		diff_eq_int("handoff V8 active calls executed", handoff_v8_calls, 2 * (v8_calls[0] + v8_calls[1]), 0);
		diff_eq_int("handoff V8 output sample denominator", handoff_v8_calls * FRAG, 2 * (v8_calls[0] + v8_calls[1]) * FRAG, 0);
		diff_eq_int("handoff downstream create admissions", handoff_admissions, 4, 0);
		printf("  handoff: V8 pairs %d/2, owners %d/%d, calls %d/%d, samples %d/%d, downstream admissions %d/4, copied rejects %d\n",
		       handoff_v8_completed, handoff_v8_created, handoff_v8_destroyed, handoff_v8_calls,
		       2 * (v8_calls[0] + v8_calls[1]), handoff_v8_calls * FRAG,
		       2 * (v8_calls[0] + v8_calls[1]) * FRAG, handoff_admissions, handoff_copy_rejected);
	}
	alarm(0);
	signal(SIGALRM, prev);
	printf("  lifecycle: %d/%d cases; %d/%d endpoint calls, %d/%d output samples, %d/%d owning destructors; payload orig/answer %d/%d bits\n",
	       completed, plan->cases, calls, plan->cases * NEP * V34_BLOCKS, calls * FRAG, plan->cases * NEP * V34_BLOCKS * FRAG,
	       destroyed, plan->cases * NEP, bits[0], bits[1]);
	return diff_end();
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
	dp_v8_init();
	g_our8 = our_ops_for(DP_V8);
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
	 * ASSERTED HERE, and the reason is worth writing down: they are HARD
	 * LINK REQUIREMENTS now, so a binary that lacked any definition would
	 * not LINK, and a runtime comparison of their addresses is a constant
	 * the compiler folds.  The link is the claim.
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

	rc |= run_lifecycles(ops34, &unobserved_lifecycles);
	if (rc) return rc;
	rc |= run_lifecycles(ops34, &observed_lifecycles);
	if (rc)
		return rc;
	rc |= handoff_control(ops8);
	if (rc) return rc;
	rc |= run_lifecycles(ops34, &handoff_lifecycles);
	if (rc) return rc;
	rc |= run_stall();
	rc |= run_queue_calls(ops34);

	return rc;
}

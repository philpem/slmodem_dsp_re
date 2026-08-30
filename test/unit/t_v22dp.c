/*
 * t_v22dp.c -- differential test of the V.22 datapump lifecycle.
 *
 *   v22_create   .text 0x004fb0  300 bytes, LOCAL
 *   v22_process  .text 0x005130  557 bytes, LOCAL
 *   dp_v22_init  .text 0x005360   72 bytes
 *   dp_v22_exit  .text 0x0053b0   70 bytes
 *   V22FP_modem  .text 0x0887b0  346 bytes
 *   V22_PROTOCOL .rodata 0x008544  28 bytes, LOCAL
 *
 * THE THREE LOCALS ARE REACHED THROUGH THE OPERATIONS TABLE, not by name --
 * `objcopy --redefine-syms` gives us a `ref_` alias for the blob's copies,
 * but OUR copies are `static` (as the object's are, and for the codegen
 * reason v22.c's header gives) so there is nothing to call.  Both sides are
 * therefore reached exactly the way the modem core reaches them: out of what
 * `dp_v22_init` registered, and `v22_process` out of the wrapper the
 * datapump built.  t_b103dp.c does the same and says why.
 *
 * ---------------------------------------------------------------------------
 * WHAT DRIVES IT, AND WHY IT IS A LINK AND NOT A TONE
 *
 * `t_b103dp.c` drives Bell 103 with a single sine at the far end's mark
 * frequency, which is enough to acquire an FSK carrier.  V.22 will not
 * acquire from a pure tone: its receiver wants a 600-baud QPSK signal with
 * the far end's scrambler running, and the state machines walk a
 * seven-node handshake with deadlines on ReadGTimer's 20 ms clock.
 *
 * So the stimulus here is the OTHER STATION: a second datapump, created with
 * the opposite `caller`, whose transmit samples are fed to this one's input
 * and vice versa.  Four sides run per block -- our originator, our answerer,
 * the blob's originator, the blob's answerer -- with the two halves of each
 * pair cross-connected and NEVER mixed across sides, so a difference is a
 * difference in one implementation and not a difference in what it was fed.
 *
 * THE THREE `.bss` STAGING BUFFERS ARE SHARED BETWEEN THE TWO INSTANCES ON
 * EACH SIDE, on both sides, because they are file-static in the object and
 * file-static here.  That is the object's own re-entrancy, reproduced: the
 * originator's block completely consumes and refills them before the
 * answerer's runs.  Running the four in a fixed order per block is what keeps
 * the two sides seeing the same sharing.
 *
 * ---------------------------------------------------------------------------
 * ANTI-VACUITY
 *
 * Two datapumps that both do nothing agree perfectly.  The run below asserts
 * that the reference side actually moved: bits were fetched, symbols were
 * handed back, the status byte took more than one value, and a line rate was
 * reported.  Without those the whole file is a tautology -- finding F134's
 * argument, and the mistake t_b103dp.c records making.
 */

#include <stdio.h>
#include <string.h>

#include "harness.h"

#include "dsplib/dp.h"
#include "dsplib/dp_wrapper.h"
#include "dsplib/v22.h"
#include "dsplib/v22fp.h"

extern int ref_dp_v22_init(void);
extern void ref_dp_v22_exit(void);

static struct dp_operations *ref_ops;
static struct dp_operations *our_ops;

static int
find_ops(void)
{
	harness_reg_reset();
	ref_dp_v22_init();
	dp_v22_init();
	if (harness_reg_ref.count < 1 || harness_reg_ours.count < 1)
		return 0;
	ref_ops = (struct dp_operations *)harness_reg_ref.ops[0];
	our_ops = (struct dp_operations *)harness_reg_ours.ops[0];
	return ref_ops != 0 && ref_ops->create != 0 && ref_ops->destroy != 0
	    && our_ops != 0 && our_ops->create != 0 && our_ops->destroy != 0;
}

/* The datapump's own process function, out of the wrapper it built. */
static dp_process_fn
process_of(struct dp *dp)
{
	return ((struct dp_wrapper *)dp->dp_data)->process;
}

/*
 * A maximal-length sequence of four-bit data words, so the transmitted data
 * is neither constant nor a short cycle at either rate.
 */
static unsigned char pattern[511];

static void
build_pattern(void)
{
	unsigned lfsr = 0x1ACEu;
	unsigned i;

	for (i = 0; i < sizeof(pattern); i++) {
		lfsr = (lfsr >> 1) ^ (-(int)(lfsr & 1u) & 0xB400u);
		pattern[i] = (unsigned char)(lfsr & 0x0fu);
	}
}

/*
 * ---------------------------------------------------------------------------
 * The registration pair.
 */
static int
check_registration(void)
{
	int rc;
	int i;

	diff_begin("dp_v22_init / dp_v22_exit");
	harness_reg_reset();
	ref_dp_v22_init();
	dp_v22_init();
	diff_eq_int("ids registered (%ld)", harness_reg_ours.count,
		    harness_reg_ref.count, 0);
	for (i = 0; i < harness_reg_ref.count && i < harness_reg_ours.count;
	     i++)
		diff_eq_int("register[%ld] id", harness_reg_ours.id[i],
			    harness_reg_ref.id[i], i);
	diff_eq_int("three ids, not two or four (%ld)",
		    harness_reg_ref.count, 3, 0);
	/*
	 * One table registered under all three ids: a copy per id would show
	 * up here as three different pointers on our side.
	 */
	for (i = 1; i < harness_reg_ours.count; i++)
		diff_eq_int("register[%ld] is the same table (%ld)",
			    (long)(harness_reg_ours.ops[i]
				   == harness_reg_ours.ops[0]), 1, i);

	harness_reg_reset();
	ref_dp_v22_exit();
	dp_v22_exit();
	diff_eq_int("ids deregistered (%ld)", harness_reg_ours.deregistered,
		    harness_reg_ref.deregistered, 0);
	for (i = 0; i < harness_reg_ref.deregistered
		    && i < harness_reg_ours.deregistered; i++)
		diff_eq_int("deregister[%ld] id", harness_reg_ours.dereg_id[i],
			    harness_reg_ref.dereg_id[i], i);
	rc = diff_end();
	return rc;
}

/*
 * ---------------------------------------------------------------------------
 * What the constructor built.  The struct the blob allocated and the struct
 * we allocated, field by field, plus the seven configuration words it derived
 * -- which is where a wrong constant would hide.
 */
static void
compare_dp(const char *what, struct v22_dp *a, struct v22_dp *b, long tag)
{
	char buf[160];

	snprintf(buf, sizeof(buf), "%s: id (%%ld)", what);
	diff_eq_int(buf, a->dp.id, b->dp.id, tag);
	snprintf(buf, sizeof(buf), "%s: status (%%ld)", what);
	diff_eq_int(buf, (long)a->dp.status, (long)b->dp.status, tag);
	snprintf(buf, sizeof(buf), "%s: modem (%%ld)", what);
	diff_eq_int(buf, (long)(a->dp.modem == b->dp.modem), 1, tag);
	snprintf(buf, sizeof(buf), "%s: bits_per_word (%%ld)", what);
	diff_eq_int(buf, a->bits_per_word, b->bits_per_word, tag);
	snprintf(buf, sizeof(buf), "%s: tx_bits_wanted (%%ld)", what);
	diff_eq_int(buf, a->tx_bits_wanted, b->tx_bits_wanted, tag);
	snprintf(buf, sizeof(buf), "%s: dp_data is the wrapper (%%ld)", what);
	diff_eq_int(buf, (long)(a->dp.dp_data == (void *)a->wrapper
				&& b->dp.dp_data == (void *)b->wrapper), 1, tag);
	snprintf(buf, sizeof(buf), "%s: wrapper->dp points back (%%ld)", what);
	diff_eq_int(buf, (long)(a->wrapper->dp == &a->dp
				&& b->wrapper->dp == &b->dp), 1, tag);

	/* The configuration, read back out of the object it built. */
	snprintf(buf, sizeof(buf), "%s: params.mode (%%ld)", what);
	diff_eq_int(buf, a->fp->params.mode, b->fp->params.mode, tag);
	snprintf(buf, sizeof(buf), "%s: params.bps (%%ld)", what);
	diff_eq_int(buf, a->fp->params.bps, b->fp->params.bps, tag);
	snprintf(buf, sizeof(buf), "%s: params.r08 (%%ld)", what);
	diff_eq_int(buf, a->fp->params.r08, b->fp->params.r08, tag);
	snprintf(buf, sizeof(buf), "%s: params.flags (%%ld)", what);
	diff_eq_int(buf, (long)a->fp->params.flags, (long)b->fp->params.flags,
		    tag);
	snprintf(buf, sizeof(buf), "%s: params.r18 (%%ld)", what);
	diff_eq_int(buf, a->fp->params.r18, b->fp->params.r18, tag);
	snprintf(buf, sizeof(buf), "%s: params.r0c, the tx gain (%%ld)", what);
	diff_eq_int(buf, a->fp->params.r0c, b->fp->params.r0c, tag);
	snprintf(buf, sizeof(buf), "%s: hdx.r04, the node deadline (%%ld)",
		 what);
	diff_eq_int(buf, a->fp->hdx->r04, b->fp->hdx->r04, tag);
	snprintf(buf, sizeof(buf), "%s: hdx.r0e, the protocol state (%%ld)",
		 what);
	diff_eq_int(buf, a->fp->hdx->r0e, b->fp->hdx->r0e, tag);
	snprintf(buf, sizeof(buf), "%s: hdx.r34, the rx shift (%%ld)", what);
	diff_eq_int(buf, a->fp->hdx->r34, b->fp->hdx->r34, tag);
	snprintf(buf, sizeof(buf), "%s: fp status byte (%%ld)", what);
	diff_eq_int(buf, (long)a->fp->status, (long)b->fp->status, tag);
	snprintf(buf, sizeof(buf), "%s: fp flags byte (%%ld)", what);
	diff_eq_int(buf, (long)a->fp->flags, (long)b->fp->flags, tag);
}

/*
 * ---------------------------------------------------------------------------
 * One side of the link: two datapumps, cross-connected.
 */
struct station {
	struct dp *org;
	struct dp *ans;
	dp_process_fn org_run;
	dp_process_fn ans_run;
	short org_out[V22_DP_FRAG];
	short ans_out[V22_DP_FRAG];
	short org_in[V22_DP_FRAG];
	short ans_in[V22_DP_FRAG];
	int org_status[512];
	int ans_status[512];
};

static struct station ours;
static struct station refs;

static int
build_side(struct station *s, struct dp_operations *op, void *m_org,
	   void *m_ans)
{
	s->org = op->create(m_org, V22_DP_ID_V22BIS, 1, V22_DP_SRATE,
			    V22_DP_FRAG, op);
	s->ans = op->create(m_ans, V22_DP_ID_V22BIS, 0, V22_DP_SRATE,
			    V22_DP_FRAG, op);
	if (s->org == 0 || s->ans == 0)
		return 0;
	s->org_run = process_of(s->org);
	s->ans_run = process_of(s->ans);
	memset(s->org_in, 0, sizeof(s->org_in));
	memset(s->ans_in, 0, sizeof(s->ans_in));
	return s->org_run != 0 && s->ans_run != 0;
}

/*
 * One block on one side.  Fixed order -- originator then answerer -- because
 * the two share the three `.bss` staging buffers.  Each station's input is
 * the other's PREVIOUS output, which is one block of loop delay and is what
 * makes the link a link rather than an echo.
 */
static void
run_block(struct station *s, int f)
{
	short org_prev[V22_DP_FRAG];
	short ans_prev[V22_DP_FRAG];

	memcpy(org_prev, s->org_out, sizeof(org_prev));
	memcpy(ans_prev, s->ans_out, sizeof(ans_prev));

	memset(s->org_out, 0x33, sizeof(s->org_out));
	memset(s->ans_out, 0x33, sizeof(s->ans_out));

	s->org_status[f] = s->org_run(s->org, s->org_in, s->org_out,
				      V22_DP_FRAG);
	s->ans_status[f] = s->ans_run(s->ans, s->ans_in, s->ans_out,
				      V22_DP_FRAG);

	memcpy(s->org_in, ans_prev, sizeof(s->org_in));
	memcpy(s->ans_in, org_prev, sizeof(s->ans_in));
}

#define BLOCKS 400

int
main(void)
{
	int rc = 0;
	int f, i;

	build_pattern();

	diff_begin("v22 registration");
	diff_eq_int("both sides registered (%ld)", find_ops(), 1, 0);
	if (ref_ops == 0 || our_ops == 0)
		return diff_end();
	rc |= diff_end();

	rc |= check_registration();

	/*
	 * v22_create, over all six (id, caller) arms.  The reference builds
	 * first so the two allocators see the same sequence.
	 */
	diff_begin("v22_create");
	{
		static const struct {
			int id;
			int caller;
			const char *name;
			int expect_mode;
			int expect_bps;
		} arms[] = {
			{ V22_DP_ID_V22BIS,  1, "V.22bis originate", 0, 2400 },
			{ V22_DP_ID_V22BIS,  0, "V.22bis answer",    1, 2400 },
			{ V22_DP_ID_V22,     1, "V.22 originate",    0, 1200 },
			{ V22_DP_ID_V22,     0, "V.22 answer",       1, 1200 },
			{ V22_DP_ID_BELL212, 1, "Bell 212 originate",0, 1200 },
			{ V22_DP_ID_BELL212, 0, "Bell 212 answer",   1, 1200 }
		};
		unsigned k;

		for (k = 0; k < sizeof(arms) / sizeof(arms[0]); k++) {
			struct dp *da, *db;

			db = ref_ops->create((void *)0x1234, arms[k].id,
					     arms[k].caller, V22_DP_SRATE,
					     V22_DP_FRAG, ref_ops);
			da = our_ops->create((void *)0x1234, arms[k].id,
					     arms[k].caller, V22_DP_SRATE,
					     V22_DP_FRAG, our_ops);
			diff_eq_int("both built (%ld)",
				    (long)(da != 0 && db != 0), 1, (long)k);
			if (da == 0 || db == 0)
				continue;

			compare_dp(arms[k].name, (struct v22_dp *)da,
				   (struct v22_dp *)db, (long)k);

			/*
			 * And against the derivation, not only against the
			 * blob: a shared misreading of `caller` or of the id
			 * table passes the comparison above and fails here.
			 */
			diff_eq_int("%ld: mode as documented",
				    ((struct v22_dp *)db)->fp->params.mode,
				    arms[k].expect_mode, (long)k);
			diff_eq_int("%ld: bit rate as documented",
				    ((struct v22_dp *)db)->fp->params.bps,
				    arms[k].expect_bps, (long)k);

			our_ops->destroy(da);
			ref_ops->destroy(db);
		}
	}
	rc |= diff_end();

	/*
	 * v22_process and V22FP_modem, over a whole handshake.
	 */
	diff_begin("v22_process over a cross-connected link");
	{
		void *m_org_a = (void *)0x1000;
		void *m_ans_a = (void *)0x2000;
		void *m_org_b = (void *)0x3000;
		void *m_ans_b = (void *)0x4000;
		int seen[256];
		int nseen = 0;

		harness_modem_reset(pattern, (int)sizeof(pattern));
		/*
		 * Four handles, four independent bit streams over the same
		 * pattern -- otherwise the two stations on one side would eat
		 * each other's data and the sides would not be comparable.
		 */
		harness_modem_route_add(m_org_a, pattern, (int)sizeof(pattern));
		harness_modem_route_add(m_ans_a, pattern, (int)sizeof(pattern));
		harness_modem_route_add(m_org_b, pattern, (int)sizeof(pattern));
		harness_modem_route_add(m_ans_b, pattern, (int)sizeof(pattern));

		memset(seen, 0, sizeof(seen));

		if (!build_side(&refs, ref_ops, m_org_b, m_ans_b)
		    || !build_side(&ours, our_ops, m_org_a, m_ans_a)) {
			diff_eq_int("both links built (%ld)", 0, 1, 0);
			rc |= diff_end();
			return rc;
		}

		for (f = 0; f < BLOCKS; f++) {
			run_block(&refs, f);
			run_block(&ours, f);

			diff_eq_int("block %ld: originate status",
				    ours.org_status[f], refs.org_status[f], f);
			diff_eq_int("block %ld: answer status",
				    ours.ans_status[f], refs.ans_status[f], f);
			for (i = 0; i < V22_DP_FRAG; i++) {
				diff_eq_int("originate sample[%ld]",
					    ours.org_out[i], refs.org_out[i], i);
				diff_eq_int("answer sample[%ld]",
					    ours.ans_out[i], refs.ans_out[i], i);
			}
			diff_eq_int("block %ld: originate fp status",
				    (long)((struct v22_dp *)ours.org)->fp->status,
				    (long)((struct v22_dp *)refs.org)->fp->status,
				    f);
			diff_eq_int("block %ld: answer fp status",
				    (long)((struct v22_dp *)ours.ans)->fp->status,
				    (long)((struct v22_dp *)refs.ans)->fp->status,
				    f);
			diff_eq_int("block %ld: originate protocol state",
				    ((struct v22_dp *)ours.org)->fp->hdx->r0e,
				    ((struct v22_dp *)refs.org)->fp->hdx->r0e,
				    f);
			diff_eq_int("block %ld: answer protocol state",
				    ((struct v22_dp *)ours.ans)->fp->hdx->r0e,
				    ((struct v22_dp *)refs.ans)->fp->hdx->r0e,
				    f);
			diff_eq_int("block %ld: originate bits_per_word",
				    ((struct v22_dp *)ours.org)->bits_per_word,
				    ((struct v22_dp *)refs.org)->bits_per_word,
				    f);
			diff_eq_int("block %ld: originate tx_bits_wanted",
				    ((struct v22_dp *)ours.org)->tx_bits_wanted,
				    ((struct v22_dp *)refs.org)->tx_bits_wanted,
				    f);

			if (!seen[((struct v22_dp *)refs.org)->fp->status]) {
				seen[((struct v22_dp *)refs.org)->fp->status] = 1;
				nseen++;
			}
		}

		/* The bit pipe, per handle, on both sides. */
		for (i = 0; i < 2; i++) {
			int ro = i;		/* route index: 0 org, 1 ans */

			diff_eq_int("route %ld: get_bits calls",
				    harness_modem_route_ours[ro].gets,
				    harness_modem_route_ref[ro + 2].gets, i);
			diff_eq_int("route %ld: put_bits calls",
				    harness_modem_route_ours[ro].puts,
				    harness_modem_route_ref[ro + 2].puts, i);
			diff_eq_int("route %ld: words handed back",
				    harness_modem_route_ours[ro].rx_len,
				    harness_modem_route_ref[ro + 2].rx_len, i);
			{
				int n = harness_modem_route_ref[ro + 2].rx_len;
				int j;

				for (j = 0; j < n; j++)
					diff_eq_int("route: word[%ld]",
					    harness_modem_route_ours[ro].rx[j],
					    harness_modem_route_ref[ro + 2].rx[j],
					    j);
			}
			diff_eq_int("route %ld: parameters set",
				    harness_modem_route_ours[ro].nparams,
				    harness_modem_route_ref[ro + 2].nparams, i);
			{
				int n = harness_modem_route_ref[ro + 2].nparams;
				int j;

				for (j = 0; j < n; j++) {
					diff_eq_int("route: param[%ld] name",
					  (long)harness_modem_route_ours[ro].param_name[j],
					  (long)harness_modem_route_ref[ro + 2].param_name[j],
					  j);
					diff_eq_int("route: param[%ld] value",
					  harness_modem_route_ours[ro].param_value[j],
					  harness_modem_route_ref[ro + 2].param_value[j],
					  j);
				}
			}
		}

		printf("  %d blocks; reference originator saw %d distinct "
		       "status values, %d get_bits, %d put_bits, %d words "
		       "back, %d params\n", BLOCKS, nseen,
		       harness_modem_route_ref[2].gets,
		       harness_modem_route_ref[2].puts,
		       harness_modem_route_ref[2].rx_len,
		       harness_modem_route_ref[2].nparams);

		/*
		 * Anti-vacuity.  Two datapumps that never left state 1 agree
		 * on everything and prove nothing.
		 */
		diff_eq_int("the machine moved (%ld distinct statuses)",
			    nseen > 1, 1, nseen);
		diff_eq_int("the data path ran (%ld get_bits)",
			    harness_modem_route_ref[2].gets > 0, 1,
			    harness_modem_route_ref[2].gets);
		diff_eq_int("words were handed back (%ld)",
			    harness_modem_route_ref[2].rx_len > 0, 1,
			    harness_modem_route_ref[2].rx_len);

		our_ops->destroy(ours.org);
		our_ops->destroy(ours.ans);
		ref_ops->destroy(refs.org);
		ref_ops->destroy(refs.ans);
	}
	rc |= diff_end();

	/* Allocation balance across the whole lifecycle. */
	diff_begin("v22_create / v22_delete balance");
	{
		struct dp *dp;
		int ra, rf, aa, af;

		harness_alloc_reset();
		dp = ref_ops->create((void *)0x1234, V22_DP_ID_V22BIS, 1,
				     V22_DP_SRATE, V22_DP_FRAG, ref_ops);
		ra = harness_alloc.allocs;
		ref_ops->destroy(dp);
		rf = harness_alloc.frees;

		harness_alloc_reset();
		dp = our_ops->create((void *)0x1234, V22_DP_ID_V22BIS, 1,
				     V22_DP_SRATE, V22_DP_FRAG, our_ops);
		aa = harness_alloc.allocs;
		our_ops->destroy(dp);
		af = harness_alloc.frees;

		printf("  ref %d allocs / %d frees, ours %d / %d\n",
		       ra, rf, aa, af);
		diff_eq_int("allocation count (%ld)", aa, ra, 0);
		diff_eq_int("free count (%ld)", af, rf, 0);
		diff_eq_int("nothing leaked (%ld)", harness_alloc.live, 0, 0);
		diff_eq_int("no bad frees (%ld)", harness_alloc.bad_free, 0, 0);
		diff_eq_int("it really allocated (%ld)", ra > 0, 1, ra);
	}
	rc |= diff_end();

	return rc;
}

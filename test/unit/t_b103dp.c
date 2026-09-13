/*
 * t_b103dp.c -- differential test of the datapump glue.
 *
 * b103_create, b103_process and b103_delete: the layer between the modem core
 * and the modulation.  Almost no arithmetic -- what it does is build the
 * configuration, shuttle bits, and translate B103's internal status into the
 * DPSTAT_* the core understands.  Each of those is a place a wrong constant
 * hides quietly.
 *
 * The bit pipe is the interesting part of the setup.  `modem_get_bits` hands
 * out data and `modem_put_bits` consumes it, so the two sides cannot share an
 * implementation -- they would eat each other's stream.  The harness gives
 * each its own cursor over the same scripted pattern and its own sink, so the
 * input is identical and the output independently observed.
 */

#include <math.h>
#include <string.h>

#include "harness.h"
#include "dsplib/b103.h"

/*
 * b103_create, b103_delete, b103_process and b103_ops are all FILE-STATIC in
 * the original, so `objcopy --redefine-syms` cannot rename them and there is
 * no `ref_b103_create` to link against.  They are reached the only way
 * anything reaches them: through what the module registers.
 *
 *   ref_dp_b103_init()  ->  the harness records the ops pointer
 *   ops->create, ops->destroy
 *   b103_process is not in the table -- `process` there is dp_wrapper_run --
 *   so it is read back out of the wrapper the datapump built.
 *
 * That is more indirect than a direct call and it is also more honest: it is
 * exactly the path the modem core takes.
 */
extern int ref_dp_b103_init(void);
extern void ref_dp_b103_exit(void);

static struct dp_operations *ref_ops;
/* Ours the same way -- `b103_ops` is file-local here too.  Finding F8121. */
static struct dp_operations *our_ops;

static int
find_ref_ops(void)
{
	harness_reg_reset();
	ref_dp_b103_init();
	dp_b103_init();
	if (harness_reg_ref.count < 1 || harness_reg_ours.count < 1)
		return 0;
	ref_ops = (struct dp_operations *)harness_reg_ref.ops[0];
	our_ops = (struct dp_operations *)harness_reg_ours.ops[0];
	return ref_ops != 0 && ref_ops->create != 0 && ref_ops->destroy != 0
	    && our_ops != 0;
}

static struct dp *
ref_b103_create(void *modem, int id, int caller, int srate, int max_frag,
		struct dp_operations *op)
{
	return ref_ops->create(modem, id, caller, srate, max_frag, op);
}

static int
ref_b103_delete(struct dp *dp)
{
	return ref_ops->destroy(dp);
}

/* Read the reference's own process function out of the wrapper it built. */
static dp_process_fn
ref_process_of(struct dp *dp)
{
	return ((struct dp_wrapper *)dp->dp_data)->process;
}

/* And OURS the same way: `b103_process` is file-static too. */
static dp_process_fn
our_process_of(struct dp *dp)
{
	return ((struct dp_wrapper *)dp->dp_data)->process;
}

/* A maximal-length sequence, so the transmitted data is not all one value. */
static unsigned char pattern[511];

static void
build_pattern(void)
{
	unsigned lfsr = 0x1ACEu;
	unsigned i;

	for (i = 0; i < sizeof(pattern); i++) {
		lfsr = (lfsr >> 1) ^ (-(int)(lfsr & 1u) & 0xB400u);
		pattern[i] = (unsigned char)(lfsr & 1);
	}
}

static void
compare_dp(const char *what, struct b103_dp *a, struct b103_dp *b, int tag)
{
	char buf[128];

	snprintf(buf, sizeof(buf), "%s: id (%%ld)", what);
	diff_eq_int(buf, a->dp.id, b->dp.id, tag);
	snprintf(buf, sizeof(buf), "%s: status (%%ld)", what);
	diff_eq_int(buf, (long)a->dp.status, (long)b->dp.status, tag);
	snprintf(buf, sizeof(buf), "%s: caller (%%ld)", what);
	diff_eq_int(buf, a->caller, b->caller, tag);
	snprintf(buf, sizeof(buf), "%s: tx_bits_wanted (%%ld)", what);
	diff_eq_int(buf, a->tx_bits_wanted, b->tx_bits_wanted, tag);
	snprintf(buf, sizeof(buf), "%s: last_status (%%ld)", what);
	diff_eq_int(buf, a->last_status, b->last_status, tag);

	/* The configuration it built is the thing most worth checking. */
	snprintf(buf, sizeof(buf), "%s: cfg.call_type (%%ld)", what);
	diff_eq_int(buf, a->fp->cfg.call_type, b->fp->cfg.call_type, tag);
	snprintf(buf, sizeof(buf), "%s: cfg.v21 (%%ld)", what);
	diff_eq_int(buf, a->fp->cfg.v21, b->fp->cfg.v21, tag);
	snprintf(buf, sizeof(buf), "%s: cfg.tone_timeout_ticks (%%ld)", what);
	diff_eq_int(buf, a->fp->cfg.tone_timeout_ticks,
		    b->fp->cfg.tone_timeout_ticks, tag);
	snprintf(buf, sizeof(buf), "%s: cfg.tx_scale (%%ld)", what);
	diff_eq_int(buf, a->fp->cfg.tx_scale, b->fp->cfg.tx_scale, tag);
	snprintf(buf, sizeof(buf), "%s: transmits mark (%%ld)", what);
	diff_eq_int(buf, a->fp->dsp->fsm.cfg.freq[1],
		    b->fp->dsp->fsm.cfg.freq[1], tag);
	snprintf(buf, sizeof(buf), "%s: bandpass taps (%%ld)", what);
	diff_eq_int(buf, a->fp->dsp->bpf_taps, b->fp->dsp->bpf_taps, tag);
}

int
main(void)
{
	static const struct {
		int id, caller;
		const char *name;
		int expect_call_type, expect_v21;
	} cases[] = {
		{ DP_B103, 1, "b103 originate", B103_CALL_ORIGINATE, 0 },
		{ DP_B103, 0, "b103 answer",    B103_CALL_ANSWER,    0 },
		{ DP_V21,  1, "v21 originate",  B103_CALL_ORIGINATE, 1 },
		{ DP_V21,  0, "v21 answer",     B103_CALL_ANSWER,    1 }
	};
	int rc = 0;
	unsigned k;

	build_pattern();

	diff_begin("b103 registration");
	diff_eq_int("the reference registered its ops (%ld)", find_ref_ops(), 1, 0);
	if (ref_ops == 0)
		return diff_end();
	rc |= diff_end();

	diff_begin("b103_create");
	for (k = 0; k < sizeof(cases) / sizeof(cases[0]); k++) {
		struct dp *da, *db;
		struct b103_dp *a, *b;

		db = ref_b103_create((void *)0x1234, cases[k].id,
				     cases[k].caller, 8000, 160, ref_ops);
		da = our_ops->create((void *)0x1234, cases[k].id,
				     cases[k].caller, 8000, 160, our_ops);
		diff_eq_int("both built (%ld)", da != 0 && db != 0, 1, (long)k);
		if (da == 0 || db == 0)
			continue;

		a = (struct b103_dp *)da;
		b = (struct b103_dp *)db;
		compare_dp(cases[k].name, a, b, (long)k);

		/*
		 * And against what the documentation claims, not only against
		 * the blob -- a shared misreading would pass the first check
		 * and fail this one.
		 */
		diff_eq_int("%ld: call_type as documented",
			    b->fp->cfg.call_type, cases[k].expect_call_type,
			    (long)k);
		diff_eq_int("%ld: v21 as documented", b->fp->cfg.v21,
			    cases[k].expect_v21, (long)k);

		our_ops->destroy(da);
		ref_b103_delete(db);
	}
	rc |= diff_end();

	/*
	 * b103_process, driven block by block through the wrapper.  The two sides get identical
	 * input -- the same scripted bits and the same samples -- and every
	 * observable is compared: the returned DPSTAT_*, the transmitted
	 * samples, the bits handed back, and the line rate reported on
	 * connect.
	 */
	diff_begin("b103_process");
	{
		static short in_a[512], in_b[512], out_a[512], out_b[512];
		struct dp *da, *db;
		int f, i;

		harness_modem_reset(pattern, (int)sizeof(pattern));
		db = ref_b103_create((void *)0x1234, DP_B103, 1, 8000, 160,
				     ref_ops);
		da = our_ops->create((void *)0x1234, DP_B103, 1, 8000, 160,
				     our_ops);

		if (da && db) {
			for (f = 0; f < 200; f++) {
				int ra, rb;

				/*
				 * 2225 Hz -- the answer channel's MARK, which
				 * is what an originating station's detector is
				 * tuned to (finding F35).  2100 Hz, the ANSam
				 * tone, is what the loopback-configured
				 * detector listens for and produces no
				 * acquisition here at all; the first version
				 * of this test used it and measured zero
				 * get_bits, which the coverage guard below now
				 * catches.
				 */
				for (i = 0; i < 160; i++) {
					double t = (f * 160.0 + i) / 8000.0;

					in_a[i] = in_b[i] = (short)(9000.0 *
						sin(2.0 * 3.14159265358979
						    * 2225.0 * t));
				}
				memset(out_a, 0x33, sizeof(out_a));
				memset(out_b, 0x33, sizeof(out_b));

				rb = ref_process_of(db)(db, in_b, out_b, 160);
				ra = our_process_of(da)(da, in_a, out_a, 160);

				diff_eq_int("block %ld: status", ra, rb, f);
				for (i = 0; i < 160; i++)
					diff_eq_int("block: sample[%ld]",
						    out_a[i], out_b[i], i);
			}

			diff_eq_int("same number of get_bits calls (%ld)",
				    harness_modem_ours.gets,
				    harness_modem_ref.gets, 0);
			diff_eq_int("same number of put_bits calls (%ld)",
				    harness_modem_ours.puts,
				    harness_modem_ref.puts, 0);
			diff_eq_int("same bits handed back (%ld)",
				    harness_modem_ours.rx_len,
				    harness_modem_ref.rx_len, 0);
			for (i = 0; i < harness_modem_ref.rx_len; i++)
				diff_eq_int("recovered bit[%ld]",
					    harness_modem_ours.rx[i],
					    harness_modem_ref.rx[i], i);
			diff_eq_int("same parameters set (%ld)",
				    harness_modem_ours.nparams,
				    harness_modem_ref.nparams, 0);
			for (i = 0; i < harness_modem_ref.nparams; i++) {
				diff_eq_int("param[%ld] name",
					    (long)harness_modem_ours.param_name[i],
					    (long)harness_modem_ref.param_name[i],
					    i);
				diff_eq_int("param[%ld] value",
					    harness_modem_ours.param_value[i],
					    harness_modem_ref.param_value[i], i);
			}
			printf("  %d get_bits, %d put_bits, %d bits back, "
			       "%d params\n", harness_modem_ref.gets,
			       harness_modem_ref.puts, harness_modem_ref.rx_len,
			       harness_modem_ref.nparams);

			/*
			 * Anti-vacuity.  Without these the run above proves
			 * only that two datapumps agreed on doing nothing --
			 * which is exactly what it did prove until the
			 * stimulus frequency was corrected.
			 */
			diff_eq_int("the data path ran (%ld)",
				    harness_modem_ref.gets > 0, 1,
				    harness_modem_ref.gets);
			diff_eq_int("bits were handed back (%ld)",
				    harness_modem_ref.rx_len > 0, 1,
				    harness_modem_ref.rx_len);
			diff_eq_int("the line rate was reported (%ld)",
				    harness_modem_ref.nparams, 2,
				    harness_modem_ref.nparams);
			for (i = 0; i < harness_modem_ref.nparams; i++)
				diff_eq_int("param[%ld] is 300 bit/s",
					    harness_modem_ref.param_value[i],
					    300, i);

			our_ops->destroy(da);
			ref_b103_delete(db);
		}
	}
	rc |= diff_end();

	/* Allocation balance, as for B103FP. */
	diff_begin("b103_create/delete balance");
	{
		struct dp *dp;
		int ra, rf, aa, af;

		harness_alloc_reset();
		dp = ref_b103_create((void *)0x1234, DP_B103, 1, 8000, 160,
				     ref_ops);
		ra = harness_alloc.allocs;
		ref_b103_delete(dp);
		rf = harness_alloc.frees;

		harness_alloc_reset();
		dp = our_ops->create((void *)0x1234, DP_B103, 1, 8000, 160,
				     our_ops);
		aa = harness_alloc.allocs;
		our_ops->destroy(dp);
		af = harness_alloc.frees;

		printf("  ref %d allocs / %d frees, ours %d / %d\n",
		       ra, rf, aa, af);
		diff_eq_int("allocation count (%ld)", aa, ra, 0);
		diff_eq_int("free count (%ld)", af, rf, 0);
		diff_eq_int("nothing leaked (%ld)", harness_alloc.live, 0, 0);
		diff_eq_int("no bad frees (%ld)", harness_alloc.bad_free, 0, 0);
	}
	rc |= diff_end();

	return rc;
}

/*
 * t_v23dp.c -- differential test of the V.23 datapump glue.
 *
 * Registration, create, delete and process: the layer between the modem core
 * and the V.23 modulation.  There is almost no arithmetic in it, which is the
 * argument for testing it rather than against.  What it does is choose -- one
 * DP_ID, one comparison on `caller` that decides which end of the call this
 * is, one status translation, one pair of line rates -- and every one of those
 * is a constant that can be wrong without anything crashing.
 *
 * `v23_create`, `v23_delete` and `v23_process` are all FILE-STATIC in the
 * original, so `objcopy --redefine-syms` cannot rename them and there is no
 * `ref_v23_create` to link against.  They are reached the only way anything
 * reaches them: out through what `dp_v23_init` registers, and -- for
 * `v23_process`, which is not in the ops table at all -- back out of the
 * wrapper the datapump built.  That is more indirect than a direct call and
 * it is also the path the modem core actually takes.
 */

#include <stdio.h>
#include <string.h>

#include "harness.h"
#include "dsplib/dp_wrapper.h"
#include "dsplib/v23.h"

extern int ref_dp_v23_init(void);
extern void ref_dp_v23_exit(void);
extern int ref_dp_wrapper_run(struct dp *dp, void *in, void *out, int count);

static struct dp_operations *ref_ops;

static int
find_ref_ops(void)
{
	harness_reg_reset();
	ref_dp_v23_init();
	if (harness_reg_ref.count < 1)
		return 0;
	ref_ops = (struct dp_operations *)harness_reg_ref.ops[0];
	return ref_ops != 0 && ref_ops->create != 0 && ref_ops->destroy != 0;
}

static struct dp *
ref_v23_create(void *modem, int id, int caller, int srate, int max_frag,
	       struct dp_operations *op)
{
	return ref_ops->create(modem, id, caller, srate, max_frag, op);
}

static int
ref_v23_delete(struct dp *dp)
{
	return ref_ops->destroy(dp);
}

static dp_process_fn
ref_process_of(struct dp *dp)
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

/*
 * Everything create decided, on both sides.  The configuration it built is
 * not reachable afterwards -- it is a stack local -- so what is compared is
 * what it produced: which receiver exists, which frequencies the transmitter
 * holds, and the two durations the answer-tone sequence was given.
 */
static void
compare_dp(const char *what, const struct v23_dp *a, const struct v23_dp *b,
	   int tag)
{
	char buf[160];

#define FIELD(name, expr) \
	do { \
		snprintf(buf, sizeof(buf), "%s: " name " (%%ld)", what); \
		diff_eq_int(buf, (long)(a->expr), (long)(b->expr), tag); \
	} while (0)

	FIELD("id", dp.id);
	FIELD("status", dp.status);
	FIELD("caller", caller);
	FIELD("tx_bits_wanted", tx_bits_wanted);
	FIELD("connected", connected);

	/* What CreateV23Modem was told, read back out of what it built. */
	FIELD("modem mode", modem->mode);
	FIELD("modem state", modem->state);
	FIELD("tone samples", modem->tone_samples);
	FIELD("silence samples", modem->silence_samples);
	FIELD("sample rate", modem->sample_rate);
	FIELD("a tone was built", modem->answer_tone != NULL);

	/* And which channel the one transmitter was pointed at. */
	FIELD("transmits mark at", modem->tx->mark);
	FIELD("transmits space at", modem->tx->space);
	FIELD("bit period[0]", modem->tx->period[0]);
	FIELD("bit period[2]", modem->tx->period[2]);
	FIELD("transmitter muted", modem->tx->mute);

#undef FIELD

	/*
	 * The receiver is two different types, so it cannot go through the
	 * macro.  Its carrier-loss timeout is the one field both have and the
	 * one this file chose, so it is compared through whichever is there.
	 */
	snprintf(buf, sizeof(buf), "%s: carrier-loss timeout (%%ld)", what);
	if (a->modem->mode != 0)
		diff_eq_int(buf,
			    ((const struct bwchdem *)a->modem->rx)->silence_limit,
			    ((const struct bwchdem *)b->modem->rx)->silence_limit,
			    tag);
	else
		diff_eq_int(buf,
			    ((const struct v23rx *)a->modem->rx)->silence_limit,
			    ((const struct v23rx *)b->modem->rx)->silence_limit,
			    tag);
}

/* Generate forward-channel FSK, using the transmitter this tree proves. */
static void
generate(short *out, int n, const int *bits, int nbits)
{
	static const short fw_period[3] = { 7, 7, 6 };
	struct v23tx *tx;
	int at = 0;
	int done = 0;

	tx = v23FP_tx_create(NULL, 1300, 2100, 3, fw_period, 0);
	while (done < n) {
		int chunk = n - done > 160 ? 160 : n - done;
		int used = 0;

		v23FP_tx_progress(tx, out + done, chunk, bits + at, &used);
		done += chunk;
		at += used;
		if (at + 32 > nbits)
			at = 0;
	}
	v23FP_tx_delete(tx);
}

#define NFRAME 300
#define NSAMP  (NFRAME * 160)

static short signal[NSAMP];

int
main(void)
{
	static int idle[64], data[64];
	int rc = 0;
	int i;

	build_pattern();
	for (i = 0; i < 64; i++) {
		idle[i] = 1;
		data[i] = (i / 2) & 1;
	}
	generate(signal, NSAMP / 4, idle, 64);
	generate(signal + NSAMP / 4, NSAMP - NSAMP / 4, data, 64);

	diff_begin("v23 registration");
	{
		struct dp_operations *ops;

		harness_reg_reset();
		ref_dp_v23_init();
		dp_v23_init();

		diff_eq_int("registration count (%ld)", harness_reg_ours.count,
			    harness_reg_ref.count, 0);
		diff_eq_int("exactly one DP_ID (%ld)", harness_reg_ours.count,
			    1, 0);
		if (harness_reg_ours.count == 1 && harness_reg_ref.count == 1) {
			diff_eq_int("the DP_ID matches (%ld)",
				    harness_reg_ours.id[0],
				    harness_reg_ref.id[0], 0);
			diff_eq_int("and it is DP_V23 (%ld)",
				    harness_reg_ours.id[0], DP_V23, 0);
		}

		ops = (struct dp_operations *)harness_reg_ref.ops[0];
		if (ops != 0) {
			diff_eq_int("name matches (%ld)",
				    strcmp(v23_ops.name, ops->name), 0, 0);
			/*
			 * The structural claim, as in t_b103_reg: the core
			 * calls the wrapper and the wrapper calls the
			 * datapump, so each side's table points at its OWN
			 * dp_wrapper_run.
			 */
			diff_eq_int("ours: process is dp_wrapper_run (%ld)",
				    (void *)v23_ops.process
				    == (void *)dp_wrapper_run, 1, 0);
			diff_eq_int("ref: process is dp_wrapper_run (%ld)",
				    (void *)ops->process
				    == (void *)ref_dp_wrapper_run, 1, 0);
			diff_eq_int("ours: no hangup handler (%ld)",
				    v23_ops.hangup == 0, 1, 0);
			diff_eq_int("ref: no hangup handler (%ld)",
				    ops->hangup == 0, 1, 0);
			diff_eq_int("use_count starts zero (%ld)",
				    v23_ops.use_count, ops->use_count, 0);
		}

		ref_dp_v23_exit();
		dp_v23_exit();
		diff_eq_int("deregistration count (%ld)",
			    harness_reg_ours.deregistered,
			    harness_reg_ref.deregistered, 0);
	}
	rc |= diff_end();

	diff_begin("v23_create: both ends");
	{
		static const struct {
			int caller;
			const char *name;
			int expect_mode, expect_mark, expect_period0;
		} cases[] = {
			/* caller != 0: this station placed the call. */
			{ 1, "terminal (originated)", 0, 390, 107 },
			{ 0, "host (answered)",       1, 1300, 7 }
		};
		unsigned k;

		diff_eq_int("the reference registered its ops (%ld)",
			    find_ref_ops(), 1, 0);
		if (ref_ops == 0)
			return diff_end();

		for (k = 0; k < sizeof(cases) / sizeof(cases[0]); k++) {
			struct dp *da, *db;

			db = ref_v23_create((void *)0x1234, DP_V23,
					    cases[k].caller, 8000, 160,
					    ref_ops);
			da = v23_create((void *)0x1234, DP_V23,
					cases[k].caller, 8000, 160, &v23_ops);
			diff_eq_int("both built (%ld)", da != 0 && db != 0, 1,
				    (long)k);
			if (da == 0 || db == 0)
				continue;

			compare_dp(cases[k].name, (struct v23_dp *)da,
				   (struct v23_dp *)db, (int)k);

			/*
			 * And against what the documentation claims, not only
			 * against the blob -- a shared misreading would pass
			 * the comparison above and fail this.
			 */
			diff_eq_int("%ld: mode as documented",
				    ((struct v23_dp *)db)->modem->mode,
				    cases[k].expect_mode, (long)k);
			diff_eq_int("%ld: transmits the documented channel",
				    ((struct v23_dp *)db)->modem->tx->mark,
				    cases[k].expect_mark, (long)k);
			diff_eq_int("%ld: with the matching bit period",
				    ((struct v23_dp *)db)->modem->tx->period[0],
				    cases[k].expect_period0, (long)k);

			v23_delete(da);
			ref_v23_delete(db);
		}
	}
	rc |= diff_end();

	diff_begin("v23_process: the terminal end, block by block");
	{
		static short in_a[512], in_b[512], out_a[512], out_b[512];
		struct dp *da, *db;
		int f;

		harness_modem_reset(pattern, (int)sizeof(pattern));
		/*
		 * caller = 1: this station originated, so it is the terminal.
		 * It receives the 1200 bps forward channel, which is what
		 * `signal` carries, and it does NOT play an answer tone -- so
		 * the datapump reaches data on the first block instead of
		 * three seconds in.
		 */
		db = ref_v23_create((void *)0x1234, DP_V23, 1, 8000, 160,
				    ref_ops);
		da = v23_create((void *)0x1234, DP_V23, 1, 8000, 160,
				&v23_ops);

		if (da != 0 && db != 0) {
			for (f = 0; f < NFRAME; f++) {
				int ra, rb;

				memcpy(in_a, signal + f * 160,
				       160 * sizeof(short));
				memcpy(in_b, signal + f * 160,
				       160 * sizeof(short));
				memset(out_a, 0x33, sizeof(out_a));
				memset(out_b, 0x33, sizeof(out_b));

				rb = ref_process_of(db)(db, in_b, out_b, 160);
				ra = v23_process(da, in_a, out_a, 160);

				diff_eq_int("block %ld: status", ra, rb, f);
				for (i = 0; i < 160; i++)
					diff_eq_int("transmitted[%ld]",
						    out_a[i], out_b[i], i);
				compare_dp("in data", (struct v23_dp *)da,
					   (struct v23_dp *)db, f);
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
			       harness_modem_ref.puts,
			       harness_modem_ref.rx_len,
			       harness_modem_ref.nparams);

			/*
			 * Anti-vacuity.  Without these the run above proves
			 * only that two datapumps agreed on doing nothing.
			 */
			diff_eq_int("the data path ran (%ld)",
				    harness_modem_ref.gets > 0, 1,
				    harness_modem_ref.gets);
			diff_eq_int("bits were handed back (%ld)",
				    harness_modem_ref.rx_len > 0, 1,
				    harness_modem_ref.rx_len);
			diff_eq_int("the line rates were reported (%ld)",
				    harness_modem_ref.nparams, 2,
				    harness_modem_ref.nparams);
			/*
			 * D24.  This end RECEIVES at 1200 and TRANSMITS at 75,
			 * and both parameters are set to 1200.  Asserted as
			 * the wrong number deliberately: it is what the
			 * original does, and a reconstruction that quietly
			 * fixed it would still pass the comparison above
			 * against a reference that does not.
			 */
			for (i = 0; i < harness_modem_ref.nparams; i++)
				diff_eq_int("param[%ld] is 1200, both ways",
					    harness_modem_ref.param_value[i],
					    V23_RATE_FORWARD, i);

			v23_delete(da);
			ref_v23_delete(db);
		}
	}
	rc |= diff_end();

	diff_begin("v23_create/delete balance");
	{
		struct dp *dp;
		int ra, rf, aa, af;

		harness_alloc_reset();
		dp = ref_v23_create((void *)0x1234, DP_V23, 1, 8000, 160,
				    ref_ops);
		ra = harness_alloc.allocs;
		ref_v23_delete(dp);
		rf = harness_alloc.frees;

		harness_alloc_reset();
		dp = v23_create((void *)0x1234, DP_V23, 1, 8000, 160,
				&v23_ops);
		aa = harness_alloc.allocs;
		v23_delete(dp);
		af = harness_alloc.frees;

		diff_eq_int("same number of allocations (%ld)", aa, ra, 0);
		diff_eq_int("same number of frees (%ld)", af, rf, 0);
		diff_eq_int("ours frees what it allocates (%ld)", aa, af, 0);
		diff_eq_int("and so does the reference (%ld)", ra, rf, 0);
	}
	rc |= diff_end();

	return rc;
}

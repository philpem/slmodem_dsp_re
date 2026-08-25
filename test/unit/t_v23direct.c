/*
 * t_v23direct.c -- v23_create, v23_process and v23_delete called BY NAME,
 * with the whole 840-byte state object compared after every block.
 *
 * The companion to t_b103direct, and for the same two reasons.
 *
 * t_v23dp reaches all three indirectly: `create` and `destroy` come out of
 * what `dp_v23_init` registers, and `process` -- which is not in the ops table
 * at all, because the core calls `dp_wrapper_run` -- is read back out of the
 * wrapper at `((struct dp_wrapper *)dp->dp_data)->process`.  Finding F221
 * removed the need for any of that: the object's file-local symbols are
 * globalized before they are renamed, so `ref_v23_process` links.
 *
 * And t_v23dp compares seventeen chosen fields, reaching through `modem` into
 * the modulation for most of them.  `struct v23_dp` is 840 bytes; 800 are
 * `tx_bits[100]` and `rx_bits[100]`, which v23_process fills as bytes, widens
 * in place to ints, and narrows back again -- three loops whose bounds nothing
 * in the suite could see.  Here the whole object is compared after every
 * block.
 *
 * Four words differ by construction -- `dp.op` is an image address, `dp_data`,
 * `modem` and `wrapper` are heap addresses -- and are checked for what can be
 * checked about them and then zeroed in a copy.  `dp.modem` is the caller's
 * own value and stays.  The modulation behind `modem` is t_v23modem's
 * subject and t_v23dp's; what is new here is the glue that owns it.
 */

#include <stdio.h>
#include <string.h>

#include "harness.h"
#include "dsplib/v23.h"

/* By name.  File-static in the object; see finding F221. */
extern struct dp *ref_v23_create(void *modem, int id, int caller, int srate,
				 int max_frag, struct dp_operations *op);
extern int ref_v23_delete(struct dp *dp);
extern int ref_v23_process(void *dp_arg, void *in, void *out, int count);

/* Only so `dp.op` can be checked against the table that side was passed. */
extern int ref_dp_v23_init(void);
extern void ref_dp_v23_exit(void);

static struct dp_operations *ref_ops;

static int
find_ref_ops(void)
{
	harness_reg_reset();
	ref_dp_v23_init();
	if (harness_reg_ref.count < 1)
		return 0;
	ref_ops = (struct dp_operations *)harness_reg_ref.ops[0];
	return ref_ops != 0;
}

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
 * The stimulus, as t_v23dp builds it: FSK from this tree's own transmitter,
 * on whichever channel the end under test listens to.  A terminal hears
 * 1300/2100; a host hears 390/450 and does not start listening until three
 * seconds of its own answer tone have gone by, so its idle run has to outlast
 * that or the demodulator never sees carrier and the comparison is vacuous.
 */
#define NFRAME     300
#define HOST_FRAME 400
#define NSAMP      (HOST_FRAME * 160)

static const short fw_period[3] = { 7, 7, 6 };
static const short bw_period[3] = { 107, 107, 106 };

static short fw_signal[NSAMP];
static short bw_signal[NSAMP];

static void
generate(short *out, int n, short mark, short space, const short *period,
	 const int *bits, int nbits)
{
	struct v23tx *tx;
	int at = 0;
	int done = 0;

	tx = v23FP_tx_create(NULL, mark, space, 3, period, 0);
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

static void
normalise(struct v23_dp *dst, const struct v23_dp *src,
	  const struct dp_operations *op, const char *side, long tag)
{
	char buf[128];

	*dst = *src;

	snprintf(buf, sizeof(buf),
		 "%s: dp.op is the table create was passed (%%ld)", side);
	diff_eq_int(buf, src->dp.op == op, 1, tag);
	snprintf(buf, sizeof(buf), "%s: dp_data is the wrapper (%%ld)", side);
	diff_eq_int(buf, src->dp.dp_data == (const void *)src->wrapper, 1, tag);
	snprintf(buf, sizeof(buf), "%s: the modulation was built (%%ld)", side);
	diff_eq_int(buf, src->modem != 0, 1, tag);

	dst->dp.op = 0;
	dst->dp.dp_data = 0;
	dst->modem = 0;
	dst->wrapper = 0;
}

static int
drive(const char *what, int caller, const short *signal, int frames)
{
	static short in_a[512], in_b[512], out_a[512], out_b[512];
	static struct v23_dp na, nb, at_create;
	struct dp *da, *db;
	int f, i, changed = 0;

	diff_begin(what);

	harness_modem_reset(pattern, (int)sizeof(pattern));
	db = ref_v23_create((void *)0x1234, DP_V23, caller, 8000, 160,
			    ref_ops);
	da = v23_create((void *)0x1234, DP_V23, caller, 8000, 160, &v23_ops);
	diff_eq_int("both built (%ld)", da != 0 && db != 0, 1, caller);
	if (da == 0 || db == 0)
		return diff_end();

	/* The constructor, whole-object, before anything has run. */
	normalise(&na, (struct v23_dp *)da, &v23_ops, "ours", -1);
	normalise(&nb, (struct v23_dp *)db, ref_ops, "ref", -1);
	diff_eq_obj("after create", struct v23_dp, &na, &nb, caller);
	at_create = na;

	for (f = 0; f < frames; f++) {
		int ra, rb;

		memcpy(in_a, signal + f * 160, 160 * sizeof(short));
		memcpy(in_b, signal + f * 160, 160 * sizeof(short));
		memset(out_a, 0x33, sizeof(out_a));
		memset(out_b, 0x33, sizeof(out_b));

		rb = ref_v23_process(db, in_b, out_b, 160);
		ra = v23_process(da, in_a, out_a, 160);

		diff_eq_int("block %ld: status", ra, rb, f);
		for (i = 0; i < 160; i++)
			diff_eq_int("transmitted[%ld]", out_a[i], out_b[i], i);

		normalise(&na, (struct v23_dp *)da, &v23_ops, "ours", f);
		normalise(&nb, (struct v23_dp *)db, ref_ops, "ref", f);
		diff_eq_obj("after block", struct v23_dp, &na, &nb, f);
		if (memcmp(&na, &at_create, sizeof(na)) != 0)
			changed = 1;
	}

	/*
	 * Anti-vacuity: two objects that never moved also compare equal, and
	 * a stimulus on the wrong channel produces exactly that.
	 */
	diff_eq_int("the state moved off what create built (%ld)", changed, 1,
		    0);
	diff_eq_int("the data path ran (%ld)", harness_modem_ref.gets > 0, 1,
		    harness_modem_ref.gets);
	diff_eq_int("bits were handed back (%ld)",
		    harness_modem_ref.rx_len > 0, 1, harness_modem_ref.rx_len);

	diff_eq_int("v23_delete returns (%ld)", v23_delete(da),
		    ref_v23_delete(db), 0);

	return diff_end();
}

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
	generate(fw_signal, NSAMP / 4, 1300, 2100, fw_period, idle, 64);
	generate(fw_signal + NSAMP / 4, NSAMP - NSAMP / 4, 1300, 2100,
		 fw_period, data, 64);
	generate(bw_signal, NSAMP * 2 / 3, 390, 450, bw_period, idle, 64);
	generate(bw_signal + NSAMP * 2 / 3, NSAMP - NSAMP * 2 / 3, 390, 450,
		 bw_period, data, 64);

	diff_begin("v23 direct: the aliases link");
	diff_eq_int("the reference registered its ops (%ld)", find_ref_ops(),
		    1, 0);
	diff_eq_int("ref_v23_create resolves (%ld)",
		    (void *)ref_v23_create != 0, 1, 0);
	diff_eq_int("ref_v23_process resolves (%ld)",
		    (void *)ref_v23_process != 0, 1, 0);
	diff_eq_int("ref_v23_delete resolves (%ld)",
		    (void *)ref_v23_delete != 0, 1, 0);
	if (ref_ops == 0)
		return diff_end();
	rc |= diff_end();

	rc |= drive("v23 terminal: whole object", 1, fw_signal, NFRAME);
	rc |= drive("v23 host: whole object", 0, bw_signal, HOST_FRAME);

	/* v23_delete by name: that it balances as well as returns. */
	diff_begin("v23_delete: balance");
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
		dp = v23_create((void *)0x1234, DP_V23, 1, 8000, 160, &v23_ops);
		aa = harness_alloc.allocs;
		v23_delete(dp);
		af = harness_alloc.frees;

		diff_eq_int("allocation count (%ld)", aa, ra, 0);
		diff_eq_int("free count (%ld)", af, rf, 0);
		diff_eq_int("nothing leaked (%ld)", harness_alloc.live, 0, 0);
		diff_eq_int("no bad frees (%ld)", harness_alloc.bad_free, 0, 0);
	}
	rc |= diff_end();

	ref_dp_v23_exit();
	return rc;
}

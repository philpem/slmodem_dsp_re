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
#include "dsplib/debug.h"

extern unsigned int ref_dsplibs_debug_level;

/*
 * By name, for the REFERENCE side: `symmap.py --globals` promotes the blob's
 * file-local symbols before renaming them, so `ref_v23_*` link.  Our three
 * are file-static and have no name to call; they are reached through the
 * table `dp_v23_init` registers and, for `v23_process`, out of the wrapper
 * `v23_create` built.  See finding F221 and `v22.c`.
 */
extern struct dp *ref_v23_create(void *modem, int id, int caller, int srate,
				 int max_frag, struct dp_operations *op);
extern int ref_v23_delete(struct dp *dp);
extern int ref_v23_process(void *dp_arg, void *in, void *out, int count);

/* Only so `dp.op` can be checked against the table that side was passed. */
extern int ref_dp_v23_init(void);
extern void ref_dp_v23_exit(void);

static struct dp_operations *ref_ops;
/* Ours the same way -- `v23_ops` is file-local.  Finding F8121. */
static struct dp_operations *our_ops;

static int
find_ref_ops(void)
{
	harness_reg_reset();
	ref_dp_v23_init();
	dp_v23_init();
	if (harness_reg_ref.count < 1 || harness_reg_ours.count < 1)
		return 0;
	ref_ops = (struct dp_operations *)harness_reg_ref.ops[0];
	our_ops = (struct dp_operations *)harness_reg_ours.ops[0];
	return ref_ops != 0 && our_ops != 0;
}

/* Our `v23_process`, out of the wrapper our `v23_create` built. */
static dp_process_fn
our_process_of(struct dp *dp)
{
	return ((struct dp_wrapper *)dp->dp_data)->process;
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
drive(const char *what, int caller, const short *signal, int frames, unsigned level, int silent_tail)
{
	static short in_a[512], in_b[512], out_a[512], out_b[512];
	static struct v23_dp na, nb, at_create;
	struct dp *da, *db;
	int f, i, changed = 0;
	int raw_zero_logged = 0, error_seen = 0, timeout_logged = 0;
	int transcripts_reported = 0;

	diff_begin(what);
	dsplibs_debug_level = ref_dsplibs_debug_level = level;
	dsplib_debug_capture_on = level != 0;
	dsplib_debug_capture_reset();

	harness_modem_reset(pattern, (int)sizeof(pattern));
	db = ref_v23_create((void *)0x1234, DP_V23, caller, 8000, 160,
			    ref_ops);
	da = our_ops->create((void *)0x1234, DP_V23, caller, 8000, 160,
				     our_ops);
	diff_eq_int("both built (%ld)", da != 0 && db != 0, 1, caller);
	if (da == 0 || db == 0)
		return diff_end();

	if (level) {
		diff_eq_int("create transcript complete (%ld)",
			dsplib_debug_capture_complete(0) && dsplib_debug_capture_complete(1), 1, level);
		diff_eq_int("create transcript equal (%ld)",
			strcmp(dsplib_debug_capture_text(0), dsplib_debug_capture_text(1)), 0, level);
		diff_eq_int("original create threshold (%ld)",
			strstr(dsplib_debug_capture_text(1), "v23: create...\n") != 0, level > 1, level);
	}

	/* The constructor, whole-object, before anything has run. */
	normalise(&na, (struct v23_dp *)da, our_ops, "ours", -1);
	normalise(&nb, (struct v23_dp *)db, ref_ops, "ref", -1);
	diff_eq_obj("after create", struct v23_dp, &na, &nb, caller);
	at_create = na;

	for (f = 0; f < frames + silent_tail; f++) {
		int ra, rb;

		if (signal && f < frames) {
			memcpy(in_a, signal + f * 160, 160 * sizeof(short));
			memcpy(in_b, signal + f * 160, 160 * sizeof(short));
		} else {
			memset(in_a, 0, 160 * sizeof(short));
			memset(in_b, 0, 160 * sizeof(short));
		}
		memset(out_a, 0x33, sizeof(out_a));
		memset(out_b, 0x33, sizeof(out_b));

		dsplib_debug_capture_reset();
		rb = ref_v23_process(db, in_b, out_b, 160);
		ra = our_process_of(da)(da, in_a, out_a, 160);

		if (rb == DPSTAT_ERROR)
			error_seen = 1;
		if (level) {
			diff_eq_int("block %ld: transcript complete",
				dsplib_debug_capture_complete(0) && dsplib_debug_capture_complete(1), 1, f);
			if (strcmp(dsplib_debug_capture_text(0), dsplib_debug_capture_text(1)) && transcripts_reported++ < 3)
				fprintf(stderr, "V23 diagnostics caller=%d level=%u block=%d\nours: %s\nref: %s\n",
					caller, level, f, dsplib_debug_capture_text(0), dsplib_debug_capture_text(1));
			diff_eq_int("block %ld: transcript equal",
				strcmp(dsplib_debug_capture_text(0), dsplib_debug_capture_text(1)), 0, f);
			if (strstr(dsplib_debug_capture_text(1), "v23: V23STAT: --> 0\n"))
				raw_zero_logged++;
			if (strstr(dsplib_debug_capture_text(1), "Carrier Detection Time Out ") ||
			    strstr(dsplib_debug_capture_text(1), "Energy drop detected......\n") ||
			    strstr(dsplib_debug_capture_text(1), "V23 no carrier detected...\r\n") ||
			    strstr(dsplib_debug_capture_text(1), "V23Debug: Timeout waiting for V23 signal...\r\n"))
				timeout_logged++;
		}
		diff_eq_int("block %ld: status", ra, rb, f);
		for (i = 0; i < 160; i++)
			diff_eq_int("transmitted[%ld]", out_a[i], out_b[i], i);

		normalise(&na, (struct v23_dp *)da, our_ops, "ours", f);
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
	if (signal) {
		diff_eq_int("the data path ran (%ld)", harness_modem_ref.gets > 0, 1,
			    harness_modem_ref.gets);
		diff_eq_int("bits were handed back (%ld)",
			    harness_modem_ref.rx_len > 0, 1, harness_modem_ref.rx_len);
	}

	if (level && signal)
		diff_eq_int("raw-zero status diagnostic threshold (%ld)", raw_zero_logged > 0, level > 1, level);
	if (!signal || silent_tail) {
		/* The blob retains carrier on this host signal-loss history even
		 * after 24 seconds. Keep that observation distinct from timeout
		 * histories; neither value nor transcript comparison is relaxed. */
		int expected_error = !signal || caller != 0;
		diff_eq_int("observed history error outcome (%ld)", error_seen, expected_error, caller);
		diff_eq_int("history timeout diagnostic threshold (%ld)",
			timeout_logged > 0, expected_error && level > 1, level);
	}
	dsplib_debug_capture_on = 0;
	dsplibs_debug_level = ref_dsplibs_debug_level = 0;
	diff_eq_int("v23_delete returns (%ld)", our_ops->destroy(da),
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

	rc |= drive("v23 terminal: whole object", 1, fw_signal, NFRAME, 0, 0);
	rc |= drive("v23 host: whole object", 0, bw_signal, HOST_FRAME, 0, 0);

	for (i = 1; i <= 3; i++) {
		char title[80];
		snprintf(title, sizeof(title), "v23 terminal diagnostics: level %d", i);
		rc |= drive(title, 1, fw_signal, NFRAME, (unsigned)i, 0);
		snprintf(title, sizeof(title), "v23 host diagnostics: level %d", i);
		rc |= drive(title, 0, bw_signal, HOST_FRAME, (unsigned)i, 0);
		snprintf(title, sizeof(title), "v23 terminal no carrier: level %d", i);
		rc |= drive(title, 1, 0, 3100, (unsigned)i, 0);
		snprintf(title, sizeof(title), "v23 host no carrier: level %d", i);
		rc |= drive(title, 0, 0, 3100, (unsigned)i, 0);
		snprintf(title, sizeof(title), "v23 terminal signal loss: level %d", i);
		rc |= drive(title, 1, fw_signal, NFRAME, (unsigned)i, 100);
		snprintf(title, sizeof(title), "v23 host retains carrier in silence: level %d", i);
		rc |= drive(title, 0, bw_signal, HOST_FRAME, (unsigned)i, 1200);
	}

	/* v23_delete through the registered table: that it balances as well as returns. */
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
		dp = our_ops->create((void *)0x1234, DP_V23, 1, 8000, 160,
				     our_ops);
		aa = harness_alloc.allocs;
		our_ops->destroy(dp);
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

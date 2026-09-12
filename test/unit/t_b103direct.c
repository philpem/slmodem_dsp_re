/*
 * t_b103direct.c -- b103_create, b103_process and b103_delete called BY NAME,
 * with the whole 840-byte state object compared after every block.
 *
 * WHY THIS IS NOT t_b103dp AGAIN
 *
 * t_b103dp reaches these three the only way that used to exist.  It calls
 * `ref_dp_b103_init`, takes `create` and `destroy` out of the registration the
 * module performs, and reads `process` back out of the `dp_wrapper` the
 * datapump built -- `((struct dp_wrapper *)dp->dp_data)->process`.  That
 * works, and it rests on the wrapper's layout: if `process` ever moved, the
 * test would happily call whatever now sits at +0x04 and compare it against
 * our `b103_process`.  Since finding F221 the object's file-local symbols are
 * globalized before they are renamed, so `ref_b103_process` links and the
 * indirection is simply gone.
 *
 * The bigger difference is what gets compared.  t_b103dp checks a returned
 * status, 160 output samples and twelve hand-picked fields.  `struct b103_dp`
 * is 840 bytes and 800 of them are the two bit buffers, which nothing looked
 * at: a block that widened one element too few, or narrowed a bit that should
 * have stayed, was invisible unless it also changed what `modem_put_bits` was
 * handed.  Here the object itself is the comparison, so a divergence surfaces
 * in the block that caused it and names the field it is in.
 *
 * FOUR WORDS DIFFER BY CONSTRUCTION
 *
 * `dp.op` is an image address, `dp.dp_data`, `fp` and `wrapper` are heap
 * addresses, and the two sides will never agree on any of them.  They are
 * zeroed in a COPY -- the layout is preserved, so tools/whichfield.py still
 * resolves any offset the comparison reports -- and each is checked for what
 * can be checked about it first: that `dp.op` is the table this side was
 * passed, that `dp_data` and `wrapper` are one pointer, that the modulation
 * was allocated at all.  Nulling a pointer without that is how a test stops
 * noticing that one side allocated and the other did not.
 *
 * `dp.modem` is the value the caller supplied, identical on both sides, so it
 * stays in the comparison.
 *
 * THE VACUITY THIS INTRODUCES
 *
 * Two objects can also agree because neither of them changed.  The guard at
 * the end compares the final state against the post-create snapshot: if
 * 200 blocks left the object exactly as `b103_create` built it, the run
 * proved nothing and says so.
 */

#include <math.h>
#include <string.h>

#include "harness.h"
#include "dsplib/b103.h"

/*
 * By name, for the REFERENCE side: --globalize-symbols promotes the blob's
 * file-local symbols before --redefine-syms aliases them, so `ref_b103_*`
 * link.  Our three are file-static and have no name to call; they are reached
 * through the table `dp_b103_init` registers and, for `b103_process`, out of
 * the wrapper `b103_create` built.  See finding F221 and `v22.c`.
 */
extern struct dp *ref_b103_create(void *modem, int id, int caller, int srate,
				  int max_frag, struct dp_operations *op);
extern int ref_b103_delete(struct dp *dp);
extern int ref_b103_process(void *dp, void *in, void *out, int count);

/*
 * The reference's own operations table.  Not needed to reach anything any
 * more -- it is here so `dp.op` can be checked against the pointer that side
 * was actually handed, rather than merely zeroed.
 */
extern int ref_dp_b103_init(void);
extern void ref_dp_b103_exit(void);

static struct dp_operations *ref_ops;
/*
 * And OURS the same way.  `b103_ops` is file-local in the original (finding
 * F8121), so our side has no more of a symbol to name than the reference's
 * does, and both tables now come out of the registration log.
 */
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
	return ref_ops != 0 && our_ops != 0;
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
 * Copy, check the four pointers, then zero them.  `side` names which
 * implementation is being normalised so a failure here says whose.
 */
static void
normalise(struct b103_dp *dst, const struct b103_dp *src,
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
	diff_eq_int(buf, src->fp != 0, 1, tag);

	dst->dp.op = 0;
	dst->dp.dp_data = 0;
	dst->fp = 0;
	dst->wrapper = 0;
}

/* Our `b103_process`, out of the wrapper our `b103_create` built. */
static dp_process_fn
our_process_of(struct dp *dp)
{
	return ((struct dp_wrapper *)dp->dp_data)->process;
}

int
main(void)
{
	static const struct {
		int id, caller;
		const char *name;
	} cases[] = {
		{ DP_B103, 1, "b103 originate" },
		{ DP_B103, 0, "b103 answer"    },
		{ DP_V21,  1, "v21 originate"  },
		{ DP_V21,  0, "v21 answer"     }
	};
	static struct b103_dp na, nb, at_create;
	int rc = 0;
	unsigned k;

	build_pattern();

	diff_begin("b103 direct: the aliases link");
	diff_eq_int("the reference registered its ops (%ld)", find_ref_ops(),
		    1, 0);
	diff_eq_int("ref_b103_create resolves (%ld)",
		    (void *)ref_b103_create != 0, 1, 0);
	diff_eq_int("ref_b103_process resolves (%ld)",
		    (void *)ref_b103_process != 0, 1, 0);
	diff_eq_int("ref_b103_delete resolves (%ld)",
		    (void *)ref_b103_delete != 0, 1, 0);
	if (ref_ops == 0)
		return diff_end();
	rc |= diff_end();

	/*
	 * The constructor, whole-object.  Every case the two arguments can
	 * select -- b103 and V.21, originating and answering -- since it is
	 * `id` and `caller` between them that build the configuration.
	 */
	diff_begin("b103_create: the whole object");
	for (k = 0; k < sizeof(cases) / sizeof(cases[0]); k++) {
		struct dp *da, *db;

		db = ref_b103_create((void *)0x1234, cases[k].id,
				     cases[k].caller, 8000, 160, ref_ops);
		da = our_ops->create((void *)0x1234, cases[k].id,
				     cases[k].caller, 8000, 160, our_ops);
		diff_eq_int("both built (%ld)", da != 0 && db != 0, 1, (long)k);
		if (da == 0 || db == 0)
			continue;

		normalise(&na, (struct b103_dp *)da, our_ops, "ours",
			  (long)k);
		normalise(&nb, (struct b103_dp *)db, ref_ops, "ref", (long)k);
		diff_eq_obj(cases[k].name, struct b103_dp, &na, &nb, (long)k);

		our_ops->destroy(da);
		ref_b103_delete(db);
	}
	rc |= diff_end();

	/*
	 * b103_process, out of the wrapper, block by block, with the state
	 * compared after each one.
	 *
	 * 2225 Hz -- the answer channel's MARK, which is what an originating
	 * station's detector is tuned to (finding F35).  t_b103dp records why
	 * 2100 Hz is the wrong choice here: it produces no acquisition at all
	 * and the run measures two datapumps agreeing on doing nothing.
	 */
	diff_begin("b103_process: the whole object, per block");
	{
		static short in_a[512], in_b[512], out_a[512], out_b[512];
		struct dp *da, *db;
		int f, i, changed = 0;

		harness_modem_reset(pattern, (int)sizeof(pattern));
		db = ref_b103_create((void *)0x1234, DP_B103, 1, 8000, 160,
				     ref_ops);
		da = our_ops->create((void *)0x1234, DP_B103, 1, 8000, 160,
				     our_ops);
		diff_eq_int("both built (%ld)", da != 0 && db != 0, 1, 0);

		if (da != 0 && db != 0) {
			normalise(&at_create, (struct b103_dp *)da, our_ops,
				  "ours", -1);

			for (f = 0; f < 200; f++) {
				int ra, rb;

				for (i = 0; i < 160; i++) {
					double t = (f * 160.0 + i) / 8000.0;

					in_a[i] = in_b[i] = (short)(9000.0 *
						sin(2.0 * 3.14159265358979
						    * 2225.0 * t));
				}
				memset(out_a, 0x33, sizeof(out_a));
				memset(out_b, 0x33, sizeof(out_b));

				rb = ref_b103_process(db, in_b, out_b, 160);
				ra = our_process_of(da)(da, in_a, out_a, 160);

				diff_eq_int("block %ld: status", ra, rb, f);
				for (i = 0; i < 160; i++)
					diff_eq_int("block: sample[%ld]",
						    out_a[i], out_b[i], i);

				normalise(&na, (struct b103_dp *)da, our_ops,
					  "ours", f);
				normalise(&nb, (struct b103_dp *)db, ref_ops,
					  "ref", f);
				diff_eq_obj("after block", struct b103_dp,
					    &na, &nb, f);
				if (memcmp(&na, &at_create, sizeof(na)) != 0)
					changed = 1;
			}

			/*
			 * Anti-vacuity.  The object comparison above is
			 * satisfied by two objects that never moved, which is
			 * exactly what a mis-tuned stimulus produces.
			 */
			diff_eq_int("the state moved off what create built "
				    "(%ld)", changed, 1, 0);
			diff_eq_int("the data path ran (%ld)",
				    harness_modem_ref.gets > 0, 1,
				    harness_modem_ref.gets);
			diff_eq_int("bits were handed back (%ld)",
				    harness_modem_ref.rx_len > 0, 1,
				    harness_modem_ref.rx_len);

			our_ops->destroy(da);
			ref_b103_delete(db);
		}
	}
	rc |= diff_end();

	/*
	 * b103_delete through the registered table: what it returns and whether it balances.  Driven
	 * separately per side because the allocator log is shared.
	 */
	diff_begin("b103_delete: return and balance");
	{
		struct dp *dp;
		int ra, rf, aa, af, rrc, arc;

		harness_alloc_reset();
		dp = ref_b103_create((void *)0x1234, DP_B103, 1, 8000, 160,
				     ref_ops);
		ra = harness_alloc.allocs;
		rrc = ref_b103_delete(dp);
		rf = harness_alloc.frees;

		harness_alloc_reset();
		dp = our_ops->create((void *)0x1234, DP_B103, 1, 8000, 160,
				     our_ops);
		aa = harness_alloc.allocs;
		arc = our_ops->destroy(dp);
		af = harness_alloc.frees;

		diff_eq_int("b103_delete returns (%ld)", arc, rrc, 0);
		diff_eq_int("allocation count (%ld)", aa, ra, 0);
		diff_eq_int("free count (%ld)", af, rf, 0);
		diff_eq_int("nothing leaked (%ld)", harness_alloc.live, 0, 0);
		diff_eq_int("no bad frees (%ld)", harness_alloc.bad_free, 0, 0);
	}
	rc |= diff_end();

	ref_dp_b103_exit();
	return rc;
}

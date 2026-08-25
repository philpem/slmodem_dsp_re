/*
 * t_v8direct.c -- v8_create, v8_process and v8_delete called BY NAME, with the
 * datapump object AND the handshake behind it compared in full.
 *
 * t_v8dp reaches create and destroy through the operations table `dp_v8_init`
 * registers, and compares eight fields of `struct v8_dp` and two of
 * `struct v8`.  `struct v8` is 3780 bytes: five transmit sequences, a receive
 * front end, a detector, a tone generator, two deadline counters and an AGC
 * line.  Two of those fields being equal says very little about the rest.
 *
 * Since finding F221 both sides can be called by name, and this file compares
 * the objects rather than a shortlist.
 *
 * POINTERS, AND WHY THEY ARE NOT SIMPLY ZEROED
 *
 * `struct v8` holds thirteen.  Eight of them aim INSIDE the object itself --
 * `tx_sym_a` and `tx_sym_b` into `tx_symbols`, `tx_ring_base` and
 * `tx_ring_half` into `tx_ring`, `rx.buf` into `rx_stage`, and `tx_seq`,
 * `seq_alt` and `seq_spare` into `seq[5]` -- so the two sides disagree only
 * about the base address.  Each is replaced, in a copy, by its BYTE OFFSET
 * from that base, which the two sides must agree on exactly.  Zeroing them
 * would have thrown away the only interesting thing about them: a pointer
 * left aiming at the wrong element of `seq[]` is precisely the defect this
 * shape of field has.
 *
 * Four point at constant tables in each side's own image -- `detector.table`
 * and the four `v21` coefficient sets -- and no comparison of the addresses is
 * possible.  Those become a 0/1, so the object comparison still fails if one
 * side installed a table and the other did not, and says nothing about which
 * table.  The tables themselves are t_v8sig's and t_v8v21's subject.
 *
 * The last is `cm`, the negotiation menu, which each side is pointed at its
 * own copy of on purpose -- sharing it would have the second side reading the
 * first's edits.  It is checked against the copy that side was given and then
 * zeroed.
 *
 * `struct v8_dp`'s five are the same argument in miniature: `self` must be the
 * object itself, `op` the table create was passed, `cm` and `dspinfo` the ones
 * this side was pointed at, and `v8` merely present.
 *
 * VACUITY
 *
 * Two objects also agree when neither has moved.  The handshake is driven from
 * a fresh create -- not from a planted state, which is what t_v8dp does and
 * what makes most of its fields equal by construction -- so the deadlines,
 * the AGC line and the receive buffers evolve on their own, and the guard at
 * the end requires that they did.
 */

#include <stdio.h>
#include <stdint.h>
#include <string.h>

#include "harness.h"
#include "dsplib/v8dp.h"
#include "dsplib/dp_param.h"
#include "dsplib/modem_params.h"

/* By name.  File-static in the object; see finding F221. */
extern struct dp *ref_v8_create(void *modem, int id, int caller, int srate,
				int max_frag, struct dp_operations *op);
extern int ref_v8_delete(struct dp *dp);
extern int ref_v8_process(struct dp *dp, void *in, void *out, int count);

/* Only so `op` can be checked against the table that side was passed. */
extern void ref_dp_v8_init(void);
extern void ref_dp_v8_exit(void);

static struct dp_operations *ref_ops;
/*
 * Ours is file-static in v8dp.c as it is in the object, so it is taken back
 * out of the registration rather than named.  `v8_create` and `v8_delete`
 * themselves are called directly.
 */
static struct dp_operations *ours_ops;

static struct dp_operations *
ops_of(int ref)
{
	struct reg_log *log = ref ? &harness_reg_ref : &harness_reg_ours;
	int i;

	harness_reg_reset();
	if (ref)
		ref_dp_v8_init();
	else
		dp_v8_init();
	for (i = 0; i < log->count; i++)
		if (log->id[i] == DP_V8)
			return log->ops[i];
	return 0;
}

static struct v8_cm cm_a, cm_b;
static struct v8_dspinfo info_a, info_b;

static struct v8_dp dp_na, dp_nb, dp_at_create;
static struct v8 v8_na, v8_nb, v8_at_create;

/*
 * A pointer aiming inside the object, replaced by its offset from the base.
 *
 * ONLY if it really does aim inside it.  `seq_spare` is not written until a
 * QCA1 message is accepted, so on a fresh object it holds the allocator's
 * fill -- 0xa5a5a5a5, HARNESS_MALLOC_FILL, and identically so on both sides.
 * Subtracting each side's own base from that turns two EQUAL values into two
 * different ones, and the first version of this file did exactly that and
 * reported a divergence at +0xc50 in every case it ran.  A value that is not
 * a pointer into the object is passed through and compared as it stands,
 * which is what makes "neither side has set this yet" visible as agreement.
 */
static void *
selfrel(const void *p, const void *base, size_t n)
{
	const char *c = p;
	const char *b = base;

	if (c < b || c >= b + n)
		return (void *)(size_t)c;
	return (void *)(size_t)(1 + (c - b));
}

static void
normalise_dp(struct v8_dp *dst, const struct v8_dp *src,
	     const struct dp_operations *op, const struct v8_cm *cm,
	     const struct v8_dspinfo *info, const char *side, long tag)
{
	char b[128];

	*dst = *src;

	snprintf(b, sizeof(b), "%s: op is the table create was passed (%%ld)",
		 side);
	diff_eq_int(b, src->op == op, 1, tag);
	snprintf(b, sizeof(b), "%s: self points at the object (%%ld)", side);
	diff_eq_int(b, src->self == src, 1, tag);
	snprintf(b, sizeof(b), "%s: cm is the menu this side was given (%%ld)",
		 side);
	diff_eq_int(b, src->cm == cm, 1, tag);
	snprintf(b, sizeof(b), "%s: dspinfo is this side's own (%%ld)", side);
	diff_eq_int(b, src->dspinfo == info, 1, tag);
	snprintf(b, sizeof(b), "%s: the handshake was built (%%ld)", side);
	diff_eq_int(b, src->v8 != 0, 1, tag);

	dst->op = 0;
	dst->self = 0;
	dst->cm = 0;
	dst->dspinfo = 0;
	dst->v8 = 0;
}

static void
normalise_v8(struct v8 *dst, const struct v8 *src, const struct v8_cm *cm,
	     const char *side, long tag)
{
	char b[128];

	*dst = *src;

	/* Inside the object: keep the offset, which both sides must agree on. */
	dst->tx_sym_a	  = selfrel(src->tx_sym_a, src, sizeof(*src));
	dst->tx_sym_b	  = selfrel(src->tx_sym_b, src, sizeof(*src));
	dst->tx_ring_base = selfrel(src->tx_ring_base, src, sizeof(*src));
	dst->tx_ring_half = selfrel(src->tx_ring_half, src, sizeof(*src));
	dst->rx.buf	  = selfrel(src->rx.buf, src, sizeof(*src));
	dst->tx_seq	  = selfrel(src->tx_seq, src, sizeof(*src));
	dst->seq_alt	  = selfrel(src->seq_alt, src, sizeof(*src));
	dst->seq_spare	  = selfrel(src->seq_spare, src, sizeof(*src));

	/* Constant tables in each side's own image: present or absent, only. */
	dst->detector.table = (const short *)(size_t)(src->detector.table != 0);
	dst->v21.a	    = (const short *)(size_t)(src->v21.a != 0);
	dst->v21.b	    = (const short *)(size_t)(src->v21.b != 0);
	dst->v21.c	    = (const short *)(size_t)(src->v21.c != 0);
	dst->v21.d	    = (const short *)(size_t)(src->v21.d != 0);

	snprintf(b, sizeof(b), "%s: v8.cm is the menu this side was given "
		 "(%%ld)", side);
	diff_eq_int(b, src->cm == cm, 1, tag);
	dst->cm = 0;
}

/* Both sides of one create, each pointed at its own menu and its own info. */
static int
build(struct dp **ours, struct dp **ref, int id, int caller, int srate)
{
	harness_param_reset();
	harness_param_set(8, 0x1f40);
	harness_param_set(5, 400);
	memset(&info_a, 0, sizeof(info_a));
	memset(&info_b, 0, sizeof(info_b));
	memset(&cm_a, 0, sizeof(cm_a));
	cm_a.b0 = 0xaa;
	cm_a.b1 = 0x55;
	cm_a.b2 = 0x50;
	cm_a.menu = 5;
	memcpy(&cm_b, &cm_a, sizeof(cm_a));

	harness_param_set(MDMPRM_DPRUNTIME, (long)(intptr_t)&cm_a);
	harness_param_set(MDMPRM_DSPINFO, (long)(intptr_t)&info_a);
	*ref = ref_v8_create((void *)0xD1A1u, id, caller, srate, 160, ref_ops);

	harness_param_set(MDMPRM_DPRUNTIME, (long)(intptr_t)&cm_b);
	harness_param_set(MDMPRM_DSPINFO, (long)(intptr_t)&info_b);
	*ours = v8_create((void *)0xD1A1u, id, caller, srate, 160, ours_ops);

	return *ours != 0 && *ref != 0;
}

static void
compare(struct dp *ours, struct dp *ref, long tag)
{
	struct v8_dp *a = (struct v8_dp *)ours;
	struct v8_dp *b = (struct v8_dp *)ref;

	normalise_dp(&dp_na, a, ours_ops, &cm_b, &info_b, "ours", tag);
	normalise_dp(&dp_nb, b, ref_ops, &cm_a, &info_a, "ref", tag);
	diff_eq_obj("datapump object", struct v8_dp, &dp_na, &dp_nb, tag);

	if (a->v8 != 0 && b->v8 != 0) {
		normalise_v8(&v8_na, a->v8, &cm_b, "ours", tag);
		normalise_v8(&v8_nb, b->v8, &cm_a, "ref", tag);
		diff_eq_obj("handshake object", struct v8, &v8_na, &v8_nb, tag);
	}
}

int
main(void)
{
	int rc = 0;
	int id, caller, srate, k;
	long built = 0, refused = 0;

	diff_begin("v8 direct: the aliases link");
	{
		ref_ops = ops_of(1);
		ours_ops = ops_of(0);
		diff_eq_int("both registered (%ld)",
			    ref_ops != 0 && ours_ops != 0, 1, 0);
		diff_eq_int("ref_v8_create resolves (%ld)",
			    (void *)ref_v8_create != 0, 1, 0);
		diff_eq_int("ref_v8_process resolves (%ld)",
			    (void *)ref_v8_process != 0, 1, 0);
		diff_eq_int("ref_v8_delete resolves (%ld)",
			    (void *)ref_v8_delete != 0, 1, 0);
	}
	if (ref_ops == 0)
		return diff_end();
	rc |= diff_end();

	/*
	 * The constructor, whole-object, over every id and end the create
	 * takes -- and three sample rates, one of which V.8 refuses, so the
	 * refusal path is walked too.
	 */
	diff_begin("v8_create: the whole object");
	for (k = 0; k < 3; k++) {
		static const int rates[] = { 9600, 8000, 48000 };

		srate = rates[k];
		for (id = 88; id <= 93; id++) {
			for (caller = 0; caller <= 1; caller++) {
				long tag = (long)(k * 64 + (id - 88) * 2
						  + caller);
				struct dp *a, *b;

				if (!build(&a, &b, id, caller, srate)) {
					diff_eq_int("both or neither (%ld)",
						    (a != 0) == (b != 0), 1,
						    tag);
					refused++;
					if (a != 0)
						v8_delete(a);
					if (b != 0)
						ref_v8_delete(b);
					continue;
				}
				built++;
				diff_eq_int("menu after create (%ld)",
					    memcmp(&cm_a, &cm_b, sizeof(cm_a))
					    == 0, 1, tag);
				compare(a, b, tag);
				v8_delete(a);
				ref_v8_delete(b);
			}
		}
	}
	diff_eq_int("pumps were built (%ld)", built > 0, 1, built);
	diff_eq_int("and refused (%ld)", refused > 0, 1, refused);
	rc |= diff_end();

	/*
	 * v8_process by name, from a fresh create, block by block.
	 *
	 * Not a planted state: what is wanted here is the handshake's own
	 * evolution -- the two deadlines counting, the AGC line filling, the
	 * receive front end advancing -- which a state written in from outside
	 * makes equal by construction.  There is no peer, so the negotiation
	 * ends in a timeout; that is a real path through the object and it
	 * takes the deadline counters with it.
	 */
	diff_begin("v8_process: the whole object, per block");
	{
		static short air[160], out_a[160], out_b[160];
		struct dp *a, *b;
		long statuses[24];
		int f, i, changed = 0, distinct = 0;

		memset(statuses, 0, sizeof(statuses));
		diff_eq_int("both built (%ld)", build(&a, &b, 90, 0, 9600), 1,
			    0);
		if (a != 0 && b != 0) {
			compare(a, b, -1);
			normalise_dp(&dp_at_create, (struct v8_dp *)a, ours_ops,
				     &cm_b, &info_b, "ours", -1);
			normalise_v8(&v8_at_create, ((struct v8_dp *)a)->v8,
				     &cm_b, "ours", -1);

			/*
			 * 900 blocks, because that is what the deadlines
			 * cost.  `fe64` advances 40 a block, `deadline_b` is
			 * 16800 and `deadline_a` 28800, so the message-detect
			 * timeout falls at block 420 and the signal-detect one
			 * at 720 -- and DPSTAT_ERROR, which is the arm of
			 * v8_process that translates them, is not reached at
			 * all before either.  A shorter run compares two
			 * datapumps that are both still waiting.
			 */
			for (f = 0; f < 900; f++) {
				int ra, rb;

				/*
				 * 2100 Hz at a level the detector responds to,
				 * which is what an originating station is
				 * listening for, plus a little dither so the
				 * AGC has something to do.
				 */
				for (i = 0; i < 160; i++) {
					int n = f * 160 + i;

					air[i] = (short)(7000.0 * __builtin_sin(
						2.0 * 3.14159265358979
						* 2100.0 * n / 8000.0)
						+ ((n * 811) % 601 - 300));
				}
				memset(out_a, 0x5a, sizeof(out_a));
				memset(out_b, 0x5a, sizeof(out_b));

				rb = ref_v8_process(b, air, out_b, 160);
				ra = v8_process(a, air, out_a, 160);

				diff_eq_int("block %ld: status", ra, rb, f);
				for (i = 0; i < 160; i++)
					diff_eq_int("block: sample[%ld]",
						    out_a[i], out_b[i], i);
				diff_eq_int("block %ld: menu",
					    memcmp(&cm_a, &cm_b, sizeof(cm_a))
					    == 0, 1, f);
				diff_eq_int("block %ld: published info",
					    memcmp(&info_a, &info_b,
						   sizeof(info_a)) == 0, 1, f);

				compare(a, b, f);
				if (memcmp(&dp_na, &dp_at_create,
					   sizeof(dp_na)) != 0
				    || memcmp(&v8_na, &v8_at_create,
					      sizeof(v8_na)) != 0)
					changed = 1;
				if (rb >= 0 && rb < 24)
					statuses[rb]++;
			}

			for (i = 0; i < 24; i++)
				if (statuses[i] != 0)
					distinct++;

			/*
			 * Anti-vacuity.  Two objects that never moved compare
			 * equal, and so do two datapumps that heard nothing.
			 */
			diff_eq_int("the handshake moved off what create built "
				    "(%ld)", changed, 1, 0);
			diff_eq_int("more than one status was returned (%ld)",
				    distinct >= 2, 1, distinct);

			diff_eq_int("v8_delete returns (%ld)", v8_delete(a),
				    ref_v8_delete(b), 0);
		}
	}
	rc |= diff_end();

	/* v8_delete by name: that it balances. */
	diff_begin("v8_delete: balance");
	{
		struct dp *a, *b;
		int ra, rf, aa, af;

		harness_alloc_reset();
		if (build(&a, &b, 90, 0, 9600)) {
			/*
			 * The two creates are counted together, so each side's
			 * share is taken by deleting one at a time.
			 */
			ra = harness_alloc.allocs;
			ref_v8_delete(b);
			rf = harness_alloc.frees;
			aa = ra;
			v8_delete(a);
			af = harness_alloc.frees - rf;

			diff_eq_int("both creates allocated the same (%ld)",
				    aa, ra, 0);
			diff_eq_int("free count (%ld)", af, rf, 0);
			diff_eq_int("nothing leaked (%ld)", harness_alloc.live,
				    0, 0);
			diff_eq_int("no bad frees (%ld)",
				    harness_alloc.bad_free, 0, 0);
		}
	}
	rc |= diff_end();

	ref_dp_v8_exit();
	dp_v8_exit();
	return rc;
}

/*
 * t_dpinit.c -- differential test of the registration aggregate and the two
 * deregistration halves this batch adds:
 *
 *   prop_dp_init / prop_dp_exit   src/core/dp_init.c   (0x0000 / 0x0030)
 *   dp_call_exit                  src/call/call.c      (0x31e0)
 *   dp_vpcm_exit                  src/pump/v90/vpcm.c  (0x4510)
 *
 * THE SPLIT THIS FILE WAS WRITTEN AGAINST HAS COLLAPSED.  All seven of the
 * aggregate's datapumps are written now (call, b103, v22, v23, v32, v8,
 * vpcm) -- v22 was the last, `src/core/dp_init.c` wiring `dp_v22_init` and
 * `dp_v22_exit` through `v22.h` in place of the local
 * `DSPLIB_DPINIT_UNWRITTEN` weak declarations it used to carry.  Nothing
 * calls the blob's copy of any of the seven from OUR side any more, so
 * `harness_reg_ref` stays empty across the whole `prop_dp_init`/
 * `prop_dp_exit` run below and every registration lands in
 * `harness_reg_ours` alone.  `is_interleaving()` still runs -- it degenerates
 * to straight equality with an empty blob side and costs nothing to keep --
 * but the explicit zero checks below are what actually PROVE the collapse
 * rather than merely tolerate it.
 */

#include <string.h>

#include "harness.h"
#include "dsplib/call.h"
#include "dsplib/dp.h"
#include "dsplib/vpcm.h"

extern int ref_prop_dp_init(void);
extern int ref_prop_dp_exit(void);
extern void ref_dp_call_init(void);
extern void ref_dp_call_exit(void);
extern int ref_dp_vpcm_init(void);
extern void ref_dp_vpcm_exit(void);

#define MAXIDS 32

/*
 * `ours` then `blob` must interleave into `want`: every id of `want` in
 * order is the next unconsumed id of exactly one of the two, and both are
 * exhausted at the end.  Returns 1 when they do.  Duplicated ids cannot
 * confuse it here: no datapump id is claimed twice, which the count checks
 * in main() establish before this runs.
 */
static int
is_interleaving(const int *want, int nwant,
		const int *ours, int nours, const int *blob, int nblob)
{
	int io = 0, ib = 0, i;

	if (nours + nblob != nwant)
		return 0;
	for (i = 0; i < nwant; i++) {
		if (io < nours && ours[io] == want[i])
			io++;
		else if (ib < nblob && blob[ib] == want[i])
			ib++;
		else
			return 0;
	}
	return io == nours && ib == nblob;
}

int
main(void)
{
	int ref_ids[MAXIDS], ref_dereg[MAXIDS];
	int nref, nref_dereg;
	int i, rc_ours, rc_ref;

	diff_begin("prop_dp registration aggregate");

	/*
	 * Reference first, alone, for the full ordered id list.
	 */
	harness_reg_reset();
	rc_ref = ref_prop_dp_init();
	nref = harness_reg_ref.count;
	diff_eq_int("ref registers into its own log only (%ld ours)",
		    harness_reg_ours.count, 0, 0);
	diff_eq_int("ref registration count sane (%ld)",
		    nref >= 7 && nref <= MAXIDS, 1, 0);
	for (i = 0; i < nref && i < MAXIDS; i++)
		ref_ids[i] = harness_reg_ref.id[i];

	rc_ours = 0;
	nref_dereg = 0;
	(void)rc_ours;

	/* No id claimed twice -- what lets is_interleaving() be simple. */
	for (i = 1; i < nref; i++) {
		int j, dup = 0;

		for (j = 0; j < i; j++)
			if (ref_ids[j] == ref_ids[i])
				dup = 1;
		diff_eq_int("ref id %ld not a duplicate", dup, 0, i);
	}

	ref_dereg[0] = 0;
	rc_ref = ref_prop_dp_exit();
	nref_dereg = harness_reg_ref.deregistered;
	diff_eq_int("ref dereg count == reg count (%ld)",
		    nref_dereg, nref, 0);
	for (i = 0; i < nref_dereg && i < MAXIDS; i++)
		ref_dereg[i] = harness_reg_ref.dereg_id[i];
	diff_eq_int("ref exit returns 0 (%ld)", rc_ref, 0, 0);

	/*
	 * Now ours.  The written pumps log to harness_reg_ours, the blob's
	 * two to harness_reg_ref; together they must be the reference run.
	 */
	harness_reg_reset();
	rc_ours = prop_dp_init();
	diff_eq_int("prop_dp_init returns 0 on both (%ld)", rc_ours, 0, 0);
	diff_eq_int("combined registration count (%ld)",
		    harness_reg_ours.count + harness_reg_ref.count, nref, 0);
	diff_eq_int("ids interleave into the reference order (%ld)",
		    is_interleaving(ref_ids, nref,
				    harness_reg_ours.id,
				    harness_reg_ours.count,
				    harness_reg_ref.id,
				    harness_reg_ref.count), 1, 0);
	/*
	 * The collapse itself: all seven datapumps are ours now, so nothing
	 * should land in the blob's log at all.  This is strictly stronger
	 * than the interleave check above, which would also pass on the old
	 * split.
	 */
	diff_eq_int("all seven are ours: nothing registers to the blob (%ld)",
		    harness_reg_ref.count, 0, 0);
	diff_eq_int("all seven are ours: our count is the whole reference (%ld)",
		    harness_reg_ours.count, nref, 0);

	/* The three VPCM ids share one table; the reference agrees. */
	{
		int n = harness_reg_ours.count;

		diff_eq_int("last three of ours are VPCM's (%ld)",
			    n >= 3
			    && harness_reg_ours.id[n - 3] == VPCM_DP_V34
			    && harness_reg_ours.id[n - 2] == VPCM_DP_V90
			    && harness_reg_ours.id[n - 1] == VPCM_DP_V92,
			    1, 0);
		diff_eq_int("ours: VPCM ids share the VPCM table (%ld)",
			    n >= 3
			    && harness_reg_ours.ops[n - 3]
			       == harness_reg_ours.ops[n - 2]
			    && harness_reg_ours.ops[n - 2]
			       == harness_reg_ours.ops[n - 1]
			    && ((struct dp_operations *)
				harness_reg_ours.ops[n - 1])->name != 0
			    && strcmp(((struct dp_operations *)
				       harness_reg_ours.ops[n - 1])->name,
				      "VPCM") == 0,
			    1, 0);
		diff_eq_int("ref: last three ids are VPCM's (%ld)",
			    ref_ids[nref - 3] == VPCM_DP_V34
			    && ref_ids[nref - 2] == VPCM_DP_V90
			    && ref_ids[nref - 1] == VPCM_DP_V92, 1, 0);
		diff_eq_int("first id is DP_CALL on both (%ld)",
			    harness_reg_ours.id[0] == DP_CALL
			    && ref_ids[0] == DP_CALL, 1, 0);
	}

	rc_ours = prop_dp_exit();
	diff_eq_int("prop_dp_exit returns 0 (%ld)", rc_ours, 0, 0);
	diff_eq_int("combined dereg count (%ld)",
		    harness_reg_ours.deregistered
		    + harness_reg_ref.deregistered, nref_dereg, 0);
	diff_eq_int("dereg ids interleave into the reference order (%ld)",
		    is_interleaving(ref_dereg, nref_dereg,
				    harness_reg_ours.dereg_id,
				    harness_reg_ours.deregistered,
				    harness_reg_ref.dereg_id,
				    harness_reg_ref.deregistered), 1, 0);
	diff_eq_int("all seven are ours: nothing deregisters from the blob (%ld)",
		    harness_reg_ref.deregistered, 0, 0);
	diff_eq_int("all seven are ours: our dereg count is the whole reference (%ld)",
		    harness_reg_ours.deregistered, nref_dereg, 0);

	/* Init and exit name the same table for the same id, both sides. */
	for (i = 0; i < harness_reg_ours.deregistered; i++) {
		int j, ok = 0;

		for (j = 0; j < harness_reg_ours.count; j++)
			if (harness_reg_ours.id[j]
			    == harness_reg_ours.dereg_id[i]
			    && harness_reg_ours.ops[j]
			    == harness_reg_ours.dereg_ops[i])
				ok = 1;
		diff_eq_int("ours dereg %ld hands back what init registered",
			    ok, 1, i);
	}

	/*
	 * The two single-module exits on their own, so a fault in either is
	 * named here rather than inside the aggregate.
	 */
	harness_reg_reset();
	dp_call_init();
	dp_call_exit();
	ref_dp_call_init();
	ref_dp_call_exit();
	diff_eq_int("call: dereg count (%ld)",
		    harness_reg_ours.deregistered,
		    harness_reg_ref.deregistered, 0);
	diff_eq_int("call: dereg id (%ld)",
		    harness_reg_ours.dereg_id[0],
		    harness_reg_ref.dereg_id[0], 0);
	diff_eq_int("call: ours deregisters what it registered (%ld)",
		    harness_reg_ours.dereg_ops[0] == harness_reg_ours.ops[0]
		    && harness_reg_ours.dereg_id[0] == harness_reg_ours.id[0],
		    1, 0);
	diff_eq_int("call: ref deregisters what it registered (%ld)",
		    harness_reg_ref.dereg_ops[0] == harness_reg_ref.ops[0]
		    && harness_reg_ref.dereg_id[0] == harness_reg_ref.id[0],
		    1, 0);

	harness_reg_reset();
	dp_vpcm_init();
	dp_vpcm_exit();
	ref_dp_vpcm_init();
	ref_dp_vpcm_exit();
	diff_eq_int("vpcm: dereg count (%ld)",
		    harness_reg_ours.deregistered,
		    harness_reg_ref.deregistered, 0);
	for (i = 0; i < 3; i++) {
		diff_eq_int("vpcm: dereg id %ld",
			    harness_reg_ours.dereg_id[i],
			    harness_reg_ref.dereg_id[i], i);
		diff_eq_int("vpcm: dereg %ld hands back the registered table",
			    harness_reg_ours.dereg_ops[i]
			    == harness_reg_ours.ops[i]
			    && harness_reg_ref.dereg_ops[i]
			    == harness_reg_ref.ops[i], 1, i);
	}

	return diff_end();
}

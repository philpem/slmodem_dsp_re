/*
 * t_b103_reg.c -- differential test of Bell 103 / V.21 registration.
 *
 * Registration is pure bookkeeping, which is exactly why it is worth testing:
 * a transposed DP_ID would not fail here, it would surface much later as the
 * wrong modulation being selected for a call.
 *
 * The ops tables cannot be compared pointer-for-pointer -- the function
 * addresses necessarily differ between the two builds -- so this compares the
 * fields that carry meaning: which IDs are claimed and in what order, the
 * datapump's name, and the crucial structural fact that `process` is the
 * wrapper rather than the datapump's own process function.
 */

#include <string.h>

#include "harness.h"
#include "dsplib/b103.h"
#include "dsplib/dp_wrapper.h"

extern int ref_dp_b103_init(void);
extern void ref_dp_b103_exit(void);
extern int ref_dp_wrapper_run(struct dp *dp, void *in, void *out, int count);

int
main(void)
{
	struct dp_operations *ref_ops;
	int i, n;

	harness_reg_reset();
	diff_begin("b103 registration");

	ref_dp_b103_init();
	dp_b103_init();

	diff_eq_int("registration count (%ld)", harness_reg_ours.count,
		    harness_reg_ref.count, 0);

	n = harness_reg_ours.count < harness_reg_ref.count
	    ? harness_reg_ours.count : harness_reg_ref.count;
	for (i = 0; i < n; i++)
		diff_eq_int("registration %ld: DP_ID", harness_reg_ours.id[i],
			    harness_reg_ref.id[i], i);

	/* Both IDs must map to the *same* ops table on each side. */
	if (n == 2) {
		diff_eq_int("ours: both IDs share one ops table (%ld)",
			    harness_reg_ours.ops[0] == harness_reg_ours.ops[1],
			    1, 0);
		diff_eq_int("ref: both IDs share one ops table (%ld)",
			    harness_reg_ref.ops[0] == harness_reg_ref.ops[1],
			    1, 0);
	}

	/* Explicit: Bell 103 and V.21, in that order. */
	if (n == 2) {
		diff_eq_int("first ID is DP_B103 (%ld)",
			    harness_reg_ours.id[0], DP_B103, 0);
		diff_eq_int("second ID is DP_V21 (%ld)",
			    harness_reg_ours.id[1], DP_V21, 0);
	}

	ref_ops = (struct dp_operations *)harness_reg_ref.ops[0];
	if (ref_ops != 0) {
		diff_eq_int("name matches (%ld)",
			    strcmp(b103_ops.name, ref_ops->name), 0, 0);

		/*
		 * The structural claim: the modem core calls the wrapper, and
		 * the wrapper calls the datapump.  Each side's ops table must
		 * point at *its own* dp_wrapper_run.
		 */
		diff_eq_int("ours: process is dp_wrapper_run (%ld)",
			    (void *)b103_ops.process == (void *)dp_wrapper_run,
			    1, 0);
		diff_eq_int("ref: process is dp_wrapper_run (%ld)",
			    (void *)ref_ops->process
			    == (void *)ref_dp_wrapper_run, 1, 0);

		diff_eq_int("ours: no hangup handler (%ld)",
			    b103_ops.hangup == 0, 1, 0);
		diff_eq_int("ref: no hangup handler (%ld)",
			    ref_ops->hangup == 0, 1, 0);
		diff_eq_int("use_count starts zero (%ld)",
			    b103_ops.use_count, ref_ops->use_count, 0);
	}

	ref_dp_b103_exit();
	dp_b103_exit();
	diff_eq_int("deregistration count (%ld)",
		    harness_reg_ours.deregistered,
		    harness_reg_ref.deregistered, 0);

	return diff_end();
}

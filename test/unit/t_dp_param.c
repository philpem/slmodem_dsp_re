/*
 * t_dp_param.c -- differential test of the datapump parameter accessor.
 *
 * Trivial in itself, but it checks the thing that actually matters: that both
 * sides request the *same* parameter index.  A reconstruction that fetched
 * MDMPRM_DSPINFO instead of MDMPRM_DPRUNTIME would return a plausible pointer
 * and fail much later, somewhere unrelated.
 */

#include "harness.h"
#include "dsplib/dp_param.h"

extern void *ref_dp_param_get(void *modem);

int
main(void)
{
	/* Distinct dummy modem pointers; never dereferenced. */
	char modem_a, modem_b;
	void *ours, *ref;

	diff_begin("dp_param_get");

	harness_param_reset();
	ours = dp_param_get(&modem_a);
	ref = ref_dp_param_get(&modem_a);

	diff_eq_int("returned value (%ld)", (long)ours, (long)ref, 0);
	diff_eq_int("parameter index (%ld)", harness_param_ours.last_param,
		    harness_param_ref.last_param, 0);
	diff_eq_int("is MDMPRM_DPRUNTIME (%ld)", harness_param_ours.last_param,
		    MDMPRM_DPRUNTIME, 0);
	diff_eq_int("call count (%ld)", harness_param_ours.calls,
		    harness_param_ref.calls, 0);
	diff_eq_int("modem passed through (%ld)",
		    harness_param_ours.last_modem == &modem_a,
		    harness_param_ref.last_modem == &modem_a, 0);

	/* A different modem pointer must reach the callback unchanged. */
	harness_param_reset();
	ours = dp_param_get(&modem_b);
	ref = ref_dp_param_get(&modem_b);
	diff_eq_int("second modem value (%ld)", (long)ours, (long)ref, 0);
	diff_eq_int("second modem pointer (%ld)",
		    harness_param_ours.last_modem == &modem_b,
		    harness_param_ref.last_modem == &modem_b, 0);

	return diff_end();
}

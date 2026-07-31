/*
 * t_v8dp.c -- differential test of the V.8 datapump wrapper.
 *
 * Both create and delete are file statics, so the way in is the operations
 * table dp_v8_init registers -- the same route t_call uses for DP_CALL.
 */

#include <stdio.h>
#include <stdint.h>
#include <string.h>

#include "harness.h"
#include "dsplib/v8dp.h"
#include "dsplib/dp_param.h"
#include "dsplib/modem_params.h"

extern void ref_dp_v8_init(void);
extern void ref_dp_v8_exit(void);

static struct v8_cm cm_a, cm_b;

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

int
main(void)
{
	int rc = 0;
	int id, caller, srate, k;
	long built = 0, refused = 0;
	struct dp_operations *oa, *ob;

	diff_begin("dp_v8_init");
	oa = ops_of(1);
	ob = ops_of(0);
	diff_eq_int("both registered", ob != 0 && oa != 0, 1, 0);
	rc |= diff_end();

	diff_begin("v8_create and v8_delete");
	if (oa == 0 || ob == 0)
		return diff_end();

	for (k = 0; k < 3; k++) {
		static const int rates[] = { 9600, 8000, 48000 };

		srate = rates[k];
		for (id = 88; id <= 93; id++) {
			for (caller = 0; caller <= 1; caller++) {
				struct dp *a, *b;
				struct v8_dp *da, *db;

				harness_param_reset();
				harness_param_set(MDMPRM_DSPINFO, 0x1234);
				harness_param_set(8, 0x2580);

				memset(&cm_a, 0x5a, sizeof(cm_a));
				memcpy(&cm_b, &cm_a, sizeof(cm_a));

				/*
				 * Both sides reach the menu through the same
				 * parameter, so each is pointed at its own
				 * copy before its turn -- otherwise the
				 * second would see the first's edits and the
				 * comparison would be meaningless.
				 */
				harness_param_set(MDMPRM_DPRUNTIME,
						  (long)(intptr_t)&cm_a);
				b = oa->create((void *)0xD1A1u, id, caller,
					       srate, 160, oa);
				harness_param_set(MDMPRM_DPRUNTIME,
						  (long)(intptr_t)&cm_b);
				a = ob->create((void *)0xD1A1u, id, caller,
					       srate, 160, ob);

				diff_eq_int("both or neither (%ld)",
					    (a != 0) == (b != 0), 1, id);
				diff_eq_int("menu after (%ld)",
					    memcmp(&cm_a, &cm_b,
						   sizeof(cm_a)) == 0, 1, id);
				if (a == 0 || b == 0) {
					refused++;
					continue;
				}
				built++;
				da = (struct v8_dp *)b;
				db = (struct v8_dp *)a;
				diff_eq_int("id (%ld)", db->id, da->id, id);
				diff_eq_int("answerer (%ld)", db->answerer,
					    da->answerer, id);
				diff_eq_int("want (%ld)", db->want, da->want,
					    id);
				diff_eq_int("dspinfo (%ld)", db->dspinfo,
					    da->dspinfo, id);
				diff_eq_int("f2c (%ld)", db->f2c, da->f2c, id);
				diff_eq_int("handshake built (%ld)",
					    db->v8 != 0, da->v8 != 0, id);
				/* The handshake itself, where it is comparable. */
				diff_eq_int("mode (%ld)", db->v8->mode,
					    da->v8->mode, id);
				diff_eq_int("timeout_a (%ld)",
					    db->v8->timeout_a,
					    da->v8->timeout_a, id);

				oa->destroy(b);
				ob->destroy(a);
			}
		}
	}
	diff_eq_int("pumps were built (%ld)", built > 0, 1, built);
	diff_eq_int("and refused (%ld)", refused > 0, 1, refused);
	rc |= diff_end();

	/*
	 * And the other end of the registration.  `dp_v8_exit` hands the same
	 * id and the same ops table back, which is the only thing it does and
	 * the only thing worth checking -- a deregistration naming a
	 * different table would leave the core holding a pointer into a
	 * datapump that thinks it has gone.
	 */
	diff_begin("dp_v8_exit");
	{
		harness_reg_reset();
		ref_dp_v8_init();
		ref_dp_v8_exit();
		dp_v8_init();
		dp_v8_exit();

		diff_eq_int("both deregistered once (%ld)",
			    harness_reg_ours.deregistered,
			    harness_reg_ref.deregistered, 0);
		diff_eq_int("reference deregistered at all (%ld)",
			    harness_reg_ref.deregistered, 1, 0);
		diff_eq_int("same id (%ld)", harness_reg_ours.dereg_id[0],
			    harness_reg_ref.dereg_id[0], 0);
		/*
		 * And the table it hands back is the one it registered --
		 * each side's own, so the pointers cannot be compared to each
		 * other, only to what that side registered.
		 */
		diff_eq_int("reference gave back what it registered (%ld)",
			    harness_reg_ref.dereg_ops[0]
			    == harness_reg_ref.ops[0], 1, 0);
		diff_eq_int("and so did ours (%ld)",
			    harness_reg_ours.dereg_ops[0]
			    == harness_reg_ours.ops[0], 1, 0);
	}
	rc |= diff_end();
	return rc;
}

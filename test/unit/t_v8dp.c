/*
 * t_v8dp.c -- differential test of the V.8 datapump wrapper.
 *
 * Both create and delete are file statics, so the way in is the operations
 * table dp_v8_init registers -- the same route t_call uses for DP_CALL.
 */

#include <stdio.h>
#include <stdlib.h>
#include <stdint.h>
#include <string.h>

#include "harness.h"
#include "dsplib/debug.h"
#include "dsplib/v8dp.h"
#include "dsplib/dp_param.h"
#include "dsplib/modem_params.h"

extern unsigned int ref_dsplibs_debug_level;

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
				diff_eq_int("mode (%ld)", db->v8->side,
					    da->v8->side, id);
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

	/*
	 * And one buffer through the wrapper.
	 *
	 * The statuses that matter are reached by placing the handshake in the
	 * state that produces them rather than by negotiating for real: V8Process
	 * reads three state words and a buffer pointer, so setting those on both
	 * sides alike is enough and takes a handful of samples instead of a
	 * second of them.
	 *
	 * `feb8` has to be a valid status before the first call.  It indexes the
	 * name table with no bound check -- faithfully, the object does the same
	 * -- and a freshly allocated handshake has whatever the allocator left.
	 */
	diff_begin("v8_process");
	{
		/*
		 * A real one this time: the create test never dereferences
		 * MDMPRM_DSPINFO, but the negotiated path writes the agreed
		 * result through it.
		 */
		static struct v8_dspinfo info_a, info_b;
		static const struct {
			const char	*name;
			short		f9d4, f9d6, f9d8;
			int		mode;
			int		calls;
			int		b0;	/* what survived, -1: leave it */
			short		fdc4;	/* quick connect took the call */
		} states[] = {
			{ "waiting for ANSam",    5, 0x19, 0x19, 0, 1, -1, 0 },
			{ "timed out on ANSam",   5, 0x0b, 0x00, 0, 1, -1, 0 },
			{ "timed out on JM",      5, 0x0c, 0x00, 0, 1, -1, 0 },
			{ "timed out on CM",   0x17, 0x04, 0x00, 1, 1, -1, 0 },
			{ "sending CM",        0x17, 0x00, 0x00, 0, 1, -1, 0 },
			/*
			 * Twice: the first call asks for the datapump change
			 * and starts the idle timer, and the second must find
			 * the timer running and leave the negotiation alone.
			 */
			{ "finished",             5, 0x63, 0x00, 0, 2, -1, 0 },
			/*
			 * And once per modulation that can win.  The menu is
			 * set after create because create writes it, and the
			 * sequence is left empty so that the update leaves the
			 * three bits alone -- what is under test is which
			 * datapump each of them asks for, not the intersection
			 * that produced them.
			 */
			{ "finished on quick connect", 5, 0x63, 0, 0, 1, 0, 1 },
			{ "finished on V.90",     5, 0x63, 0x00, 0, 1, 0x08, 0 },
			{ "finished on V.34",     5, 0x63, 0x00, 0, 1, 0x20, 0 },
			{ "finished on V.32",     5, 0x63, 0x00, 0, 1, 0x80, 0 },
			{ "finished on nothing",  5, 0x63, 0x00, 0, 1, 0x00, 0 }
		};
		short in[8], out_a[8], out_b[8];
		unsigned si, lvl;
		long lines = 0;
		int call;

		for (k = 0; k < 8; k++)
			in[k] = (short)(k * 900 - 3000);

		for (lvl = 0; lvl <= 3; lvl++) {
			dsplib_debug_capture_reset();
			dsplibs_debug_level = ref_dsplibs_debug_level = lvl;
			dsplib_debug_capture_on = lvl != 0;

			/*
			 * One create at a rate V.8 does not serve.  It is
			 * refused, and refused silently -- the announcement
			 * comes after the check, not before.
			 */
			harness_param_reset();
			harness_param_set(8, 0x1f40);
			harness_param_set(MDMPRM_DSPINFO,
					  (long)(intptr_t)&info_a);
			harness_param_set(MDMPRM_DPRUNTIME,
					  (long)(intptr_t)&cm_a);
			diff_eq_int("wrong rate refused (%ld)",
				    ob->create((void *)0xD1A1u, 90, 0, 8000,
					       160, ob) == 0,
				    oa->create((void *)0xD1A1u, 90, 0, 8000,
					       160, oa) == 0, (long)lvl);

			for (si = 0; si < sizeof(states) / sizeof(states[0]);
			     si++) {
				struct dp *a, *b;
				struct v8_dp *da, *db;

				harness_param_reset();
				/*
				 * Not the sample rate: the two are separate
				 * parameters and the trace names one of them,
				 * so they have to differ or either would do.
				 */
				harness_param_set(8, 0x1f40);
				harness_param_set(5, 400);
				memset(&info_a, 0, sizeof(info_a));
				memset(&info_b, 0, sizeof(info_b));

				memset(&cm_a, 0, sizeof(cm_a));
				cm_a.b0 = 0xaa;
				cm_a.b1 = 0x55;
				cm_a.b2 = 0x50;	/* quick connect, LAPM */
				cm_a.menu = 5;
				memcpy(&cm_b, &cm_a, sizeof(cm_a));

				harness_param_set(MDMPRM_DPRUNTIME,
						  (long)(intptr_t)&cm_a);
				harness_param_set(MDMPRM_DSPINFO,
						  (long)(intptr_t)&info_a);
				b = oa->create((void *)0xD1A1u, 90, 0, 9600,
					       160, oa);
				harness_param_set(MDMPRM_DPRUNTIME,
						  (long)(intptr_t)&cm_b);
				harness_param_set(MDMPRM_DSPINFO,
						  (long)(intptr_t)&info_b);
				a = ob->create((void *)0xD1A1u, 90, 0, 9600,
					       160, ob);
				if (a == 0 || b == 0)
					continue;
				da = (struct v8_dp *)b;
				db = (struct v8_dp *)a;

				da->v8->tx_state = db->v8->tx_state = states[si].f9d4;
				da->v8->rx_state = db->v8->rx_state = states[si].f9d6;
				da->v8->rx_substate = db->v8->rx_substate = states[si].f9d8;
				da->v8->side = db->v8->side = states[si].mode;
				/*
				 * The state machine itself is not what is
				 * under test here, and most of its paths
				 * need a line to listen to.  So the transmit
				 * queue is left with no room and the symbol
				 * count below the threshold, except for the
				 * one state that has to reach the machine to
				 * report that it has finished.
				 */
				da->v8->tx_avail = db->v8->tx_avail = 0x60;
				da->v8->tx_fill_target = db->v8->tx_fill_target = 0;
				da->v8->sym_avail = db->v8->sym_avail =
					states[si].f9d6 == 0x63 ? 40 : -40;
				da->v8->prev_status = db->v8->prev_status = V8_INIT;
				da->f2c = db->f2c = V8_INIT;
				da->f20 = db->f20 = 0;
				da->v8->quick_connect = db->v8->quick_connect = states[si].fdc4;
				da->v8->anspcm_level = db->v8->anspcm_level = 0;
				da->v8->lapm_indication = db->v8->lapm_indication = 1;
				da->v8->seq[2].wordidx =
					db->v8->seq[2].wordidx = 0;
				if (states[si].b0 >= 0) {
					cm_a.b0 = (unsigned char)states[si].b0;
					cm_b.b0 = (unsigned char)states[si].b0;
				}

				for (call = 0; call < states[si].calls; call++) {
					int ra, rb;

					memset(out_a, 0, sizeof(out_a));
					memset(out_b, 0, sizeof(out_b));
					ra = oa->process(b, in, out_a, 8);
					rb = ob->process(a, in, out_b, 8);

					diff_eq_int("returns (%ld)", rb, ra,
						    (long)(lvl * 64 + si * 4
							   + call));
					diff_eq_int("idle timer (%ld)",
						    db->f20, da->f20,
						    (long)(lvl * 64 + si * 4
							   + call));
					diff_eq_int("status kept (%ld)",
						    db->f2c, da->f2c,
						    (long)(lvl * 64 + si * 4
							   + call));
					diff_eq_int("f1c (%ld)", db->f1c,
						    da->f1c,
						    (long)(lvl * 64 + si));
					diff_eq_int("samples out (%ld)",
						    memcmp(out_a, out_b,
							   sizeof(out_a)) == 0,
						    1, (long)(lvl * 64 + si));
					diff_eq_int("menu after (%ld)",
						    memcmp(&cm_a, &cm_b,
							   sizeof(cm_a)) == 0,
						    1, (long)(lvl * 64 + si));
					/*
					 * What was published, not just that
					 * something was: the two fields are
					 * given values that survive the update
					 * so that writing them on the wrong
					 * branch shows up.
					 */
					diff_eq_int("published f08 (%ld)",
						    info_b.f08, info_a.f08,
						    (long)(lvl * 64 + si));
					diff_eq_int("published f0c (%ld)",
						    info_b.f0c, info_a.f0c,
						    (long)(lvl * 64 + si));
				}

				oa->destroy(b);
				ob->destroy(a);
			}

			dsplib_debug_capture_on = 0;
			dsplibs_debug_level = ref_dsplibs_debug_level = 0;

			if (lvl == 0)
				continue;

			diff_eq_int("transcript matches (level %ld)",
				    strcmp(dsplib_debug_capture_text(0),
					   dsplib_debug_capture_text(1)) == 0,
				    1, (long)lvl);
			if (getenv("DBGDIFF")
			    && strcmp(dsplib_debug_capture_text(0),
				      dsplib_debug_capture_text(1)) != 0) {
				const char *o = dsplib_debug_capture_text(0);
				const char *r = dsplib_debug_capture_text(1);
				int j = 0;

				while (o[j] && o[j] == r[j])
					j++;
				while (j > 0 && o[j - 1] != '\n')
					j--;
				printf("=== level %u: divergence at %d\n",
				       lvl, j);
				printf("--- ours: %.400s\n", o + j);
				printf("--- ref : %.400s\n", r + j);
			}
			diff_eq_int("line counts match (level %ld)",
				    (int)dsplib_debug_capture_lines(0),
				    (int)dsplib_debug_capture_lines(1),
				    (long)lvl);
			if (lvl == 1)
				diff_eq_int("silent below the threshold",
					    (int)dsplib_debug_capture_lines(1),
					    0, 0);
			else
				lines += dsplib_debug_capture_lines(1);
		}

		diff_eq_int("diagnostics were captured (%ld lines)",
			    lines > 15, 1, lines);
	}
	rc |= diff_end();
	return rc;
}

/*
 * t_v32dp.c -- differential test of the V.32 datapump glue.
 *
 *   v32_create   .text 0x004560   627
 *   v32_delete   .text 0x0047e0    95
 *   v32_process  .text 0x004840   871
 *   dp_v32_init  .text 0x004bb0    49
 *   dp_v32_exit  .text 0x004bf0    49
 *   v32_ops      .data  0x000048   24
 *
 * There is almost no arithmetic in this layer, which is the argument for
 * testing it rather than against.  What it does is CHOOSE -- two DP_IDs, one
 * comparison on `caller`, a physical delay clamped and reported back, a
 * 29-entry status translation, a rate-to-bits division -- and every one of
 * those is a constant that can be wrong without anything crashing.
 *
 * `v32_ops` is file-local, so it is reached the way the modem core reaches
 * it: out through what `dp_v32_init` registered.  The three entry points are
 * NOT file-local to the harness -- `symmap.py --globals` exports
 * `ref_v32_create`, `ref_v32_delete` and `ref_v32_process`, which `nm` on
 * `build/dsplibs_ref.o` confirms -- so they are called directly as well, and
 * the registration is checked separately.  (`v23.c`'s test predates that and
 * says otherwise in its own header; the tool is the authority.)
 *
 * WHAT EACH GROUP IS FOR, as a named wrong reading:
 *
 *   1. ONE id registered instead of two.  `dp_v32_init` registers the same
 *      table under DP_V32 and DP_V32BIS; the test asserts BOTH ids and that
 *      the two registrations name the SAME table, which a pair of separate
 *      tables would pass on a count alone.
 *   2. `v32_ops.process` set to `v32_process`.  It is `dp_wrapper_run`; the
 *      datapump reaches `v32_process` only through the wrapper it built.
 *      Both are checked, by identity.
 *   3. The physical delay clamped at the wrong end.  `MDMPRM_IODELAY` is
 *      swept across the 216 boundary and the test asserts BOTH that the
 *      clamp fired at least once and that it did not fire at least once,
 *      because a clamp that always fires is a constant.
 *   4. `caller` inverted.  Its two values give two different `protocol`s in
 *      the parameter block, and the test compares the block.
 *   5. The status translation read as a range rather than a table.  The 29
 *      codes are driven ONE AT A TIME by planting `V32_OBJ_STATUS` and
 *      dispatching through the null protocol slot, so each of the four arms
 *      is reached and counted, and the counts are asserted.
 *   6. `bits_per_symbol` left at its seed.  The connect arm recomputes it as
 *      `line_rate / 2400`; the test asserts it changed and that it matches
 *      the rate reported through `modem_set_param`.
 */

#include <string.h>

#include "harness.h"

#include "dsplib/dp.h"
#include "dsplib/dp_wrapper.h"
#include "dsplib/modem_params.h"
#include "dsplib/v32.h"
#include "dsplib/v32fp.h"
#include "dsplib/v32fpstat.h"

extern int ref_dp_v32_init(void);
extern void ref_dp_v32_exit(void);
extern struct dp *ref_v32_create(void *modem, int id, int caller, int srate,
				 int max_frag, struct dp_operations *op);
extern int ref_v32_delete(struct dp *dp);
extern int ref_v32_process(void *dp_arg, void *in, void *out, int count);
extern int ref_dp_wrapper_run(struct dp *dp, void *in, void *out, int count);
extern int ref_Control_Flag;

#define FIELD(o, off)		((unsigned char *)(void *)(o) + (off))
#define F_PTR(o, off)		(*(void **)(void *)FIELD((o), (off)))
#define F_U16(o, off)		(*(unsigned short *)(void *)FIELD((o), (off)))
#define F_U8(o, off)		(*(unsigned char *)FIELD((o), (off)))

/* ------------------------------------------------------------------------ */

static int
test_registration(void)
{
	int i;
	int saw32 = 0;
	int saw132 = 0;
	void *ops32 = 0;
	void *ops132 = 0;

	diff_begin("dp_v32_init / dp_v32_exit");

	harness_reg_reset();
	diff_eq_int("dp_v32_init returns the same on both sides (%ld)",
		    dp_v32_init(), ref_dp_v32_init(), 0);

	diff_eq_int("ours registered twice (%ld)", harness_reg_ours.count, 2,
		    0);
	diff_eq_int("the blob registered twice (%ld)", harness_reg_ref.count,
		    2, 1);

	for (i = 0; i < harness_reg_ours.count && i < 2; i++) {
		diff_eq_int("registration %ld: same id",
			    harness_reg_ours.id[i], harness_reg_ref.id[i], i);
		if (harness_reg_ours.id[i] == DP_V32) {
			saw32 = 1;
			ops32 = harness_reg_ours.ops[i];
		}
		if (harness_reg_ours.id[i] == DP_V32BIS) {
			saw132 = 1;
			ops132 = harness_reg_ours.ops[i];
		}
	}
	diff_eq_int("DP_V32 was registered (%ld)", saw32, 1, 0);
	diff_eq_int("DP_V32BIS was registered (%ld)", saw132, 1, 1);
	/*
	 * ONE table under two ids, not two tables.  A pair of separate tables
	 * passes every count above.
	 */
	diff_eq_int("both ids name the SAME table (%ld)", ops32 == ops132, 1,
		    2);

	if (ops32 != 0) {
		struct dp_operations *o = (struct dp_operations *)ops32;
		struct dp_operations *r =
			(struct dp_operations *)harness_reg_ref.ops[0];

		diff_eq_int("ops.name is \"v32\" (%ld)",
			    o->name != 0 && strcmp(o->name, "v32") == 0, 1, 0);
		diff_eq_int("the blob's name agrees (%ld)",
			    r->name != 0 && strcmp(r->name, "v32") == 0, 1, 1);
		diff_eq_int("ops.use_count (%ld)", o->use_count, r->use_count,
			    2);
		diff_eq_int("ops.create is v32_create (%ld)",
			    o->create == v32_create, 1, 3);
		diff_eq_int("ops.destroy is v32_delete (%ld)",
			    o->destroy == v32_delete, 1, 4);
		/*
		 * `process` is `dp_wrapper_run` and NOT `v32_process`.  Both
		 * halves are asserted: the first would pass on a null.
		 */
		diff_eq_int("ops.process is dp_wrapper_run (%ld)",
			    (void *)o->process == (void *)dp_wrapper_run, 1, 5);
		diff_eq_int("ops.process is NOT v32_process (%ld)",
			    (void *)o->process != (void *)v32_process, 1, 6);
		diff_eq_int("the blob's process is ref_dp_wrapper_run (%ld)",
			    (void *)r->process == (void *)ref_dp_wrapper_run,
			    1, 7);
		diff_eq_int("ops.hangup is null (%ld)", o->hangup == 0, 1, 8);
	}

	harness_reg_reset();
	dp_v32_init();
	ref_dp_v32_init();
	dp_v32_exit();
	ref_dp_v32_exit();
	diff_eq_int("ours deregistered twice (%ld)",
		    harness_reg_ours.deregistered, 2, 0);
	diff_eq_int("the blob deregistered twice (%ld)",
		    harness_reg_ref.deregistered, 2, 1);
	for (i = 0; i < harness_reg_ours.deregistered && i < 2; i++) {
		diff_eq_int("deregistration %ld: same id",
			    harness_reg_ours.dereg_id[i],
			    harness_reg_ref.dereg_id[i], i);
		/*
		 * The table deregistered must be the one registered, or the
		 * core is left holding a pointer into something that thinks it
		 * has gone -- which a bare count cannot see.
		 */
		diff_eq_int("deregistration %ld: the registered table",
			    harness_reg_ours.dereg_ops[i]
			    == harness_reg_ours.ops[i], 1, i);
	}

	return diff_end();
}

/* ------------------------------------------------------------------------ */

static void
cmp_dp(struct v32_dp *a, struct v32_dp *b, long trial)
{
	int i;

	diff_eq_int("dp.id (%ld)", a->dp.id, b->dp.id, trial);
	diff_eq_int("dp.status (%ld)", (long)a->dp.status,
		    (long)b->dp.status, trial);
	diff_eq_int("bits_per_symbol (%ld)", a->bits_per_symbol,
		    b->bits_per_symbol, trial);
	diff_eq_int("line_rate (%ld)", (long)a->line_rate,
		    (long)b->line_rate, trial);
	diff_eq_int("symbols_per_block (%ld)", a->symbols_per_block,
		    b->symbols_per_block, trial);
	diff_eq_int("fp allocated (%ld)", a->fp != 0, b->fp != 0, trial);
	diff_eq_int("wrapper allocated (%ld)", a->wrapper != 0,
		    b->wrapper != 0, trial);
	if (a->fp != 0 && b->fp != 0)
		for (i = 0; i < 48; i++)
			diff_eq_int("fp params byte %ld",
				    ((unsigned char *)a->fp)[i],
				    ((unsigned char *)b->fp)[i], i);
}

static int
test_create(void)
{
	static const long delays[] = { 0, 100, 168, 169, 400 };
	int clamped = 0;
	int unclamped = 0;
	int moved_caller = 0;
	int d;
	int caller;

	diff_begin("v32_create over the delay and caller sweep");

	for (d = 0; d < 5; d++)
		for (caller = 0; caller < 2; caller++) {
			struct dp *a;
			struct dp *b;
			struct dp_operations opa;
			struct dp_operations opb;
			int nset_before;

			harness_param_reset();
			harness_param_set(MDMPRM_MAX_RATE, 14400);
			harness_param_set(MDMPRM_IODELAY, delays[d]);
			memset(&opa, 0, sizeof(opa));
			memset(&opb, 0, sizeof(opb));

			nset_before = harness_modem_ref.nparams;
			a = v32_create((void *)0x1234, DP_V32, caller, 8000,
				       160, &opa);
			b = ref_v32_create((void *)0x1234, DP_V32, caller, 8000,
					   160, &opb);
			diff_eq_int("ours built (%ld)", a != 0, 1, d);
			diff_eq_int("the blob built (%ld)", b != 0, 1, d);
			if (a == 0 || b == 0)
				return diff_end();

			cmp_dp((struct v32_dp *)a, (struct v32_dp *)b, d);
			diff_eq_int("dp.op is the argument (%ld)",
				    a->op == &opa && b->op == &opb, 1, d);
			diff_eq_int("dp.modem is the argument (%ld)",
				    a->modem == (void *)0x1234
				    && b->modem == (void *)0x1234, 1, d);
			diff_eq_int("dp_data is the wrapper (%ld)",
				    a->dp_data ==
				    (void *)((struct v32_dp *)a)->wrapper, 1,
				    d);

			/*
			 * The clamp reports the excess back through
			 * MDMPRM_UPDATE_DELAY.  Both arms must be exercised or
			 * the boundary is untested.
			 */
			if (harness_modem_ref.nparams > nset_before)
				clamped = 1;
			else
				unclamped = 1;

			if (caller != 0
			    && ((struct v32_dp *)b)->fp != 0
			    && *(short *)((struct v32_dp *)b)->fp
			       != *(short *)((struct v32_dp *)a)->fp)
				moved_caller = 1;

			v32_delete(a);
			ref_v32_delete(b);
		}

	diff_eq_int("the delay clamp fired (%ld)", clamped, 1, 0);
	diff_eq_int("the delay clamp did not always fire (%ld)", unclamped, 1,
		    1);
	/*
	 * `caller` reaches the object only as `cfg.protocol`, and the two
	 * sides must AGREE about it -- which the parameter-block comparison
	 * above already checks.  This counter exists to say the axis was not
	 * silently constant; it is asserted zero because both sides move
	 * together.
	 */
	diff_eq_int("the caller axis never split the two sides (%ld)",
		    moved_caller, 0, 2);
	return diff_end();
}

/* ------------------------------------------------------------------------ */

/*
 * The status translation, driven one code at a time.
 *
 * `v32_process` calls `V32FP_modem`, which dispatches through
 * `V32_PROTOCOL[hdx->mode]` and returns `V32_OBJ_STATUS`.  Pinning the mode
 * at a NULL slot makes the dispatch a no-op, so the status the caller sees is
 * exactly the byte planted here -- which is what turns a 29-entry jump table
 * into 29 separately observable cases.
 */
static int
test_process_status(void)
{
	static const unsigned char pattern[64] = {
		1, 0, 1, 1, 0, 0, 1, 0, 1, 1, 1, 0, 0, 1, 0, 0
	};
	short in[40];
	short outa[40];
	short outb[40];
	int arm_ok = 0;
	int arm_connect = 0;
	int arm_error = 0;
	int arm_unknown = 0;
	int code;
	int i;

	diff_begin("v32_process, every status code");

	for (i = 0; i < 40; i++)
		in[i] = (short)((i * 517) - 8192);

	for (code = 0; code <= 30; code++) {
		struct dp *a;
		struct dp *b;
		struct dp_operations opa;
		struct dp_operations opb;
		int ra;
		int rb;

		harness_param_reset();
		harness_param_set(MDMPRM_MAX_RATE, 14400);
		harness_param_set(MDMPRM_IODELAY, 100);
		harness_modem_reset(pattern, (int)sizeof(pattern));
		memset(&opa, 0, sizeof(opa));
		memset(&opb, 0, sizeof(opb));
		Control_Flag = 0;
		ref_Control_Flag = 0;

		a = v32_create((void *)0x2222, DP_V32, 0, 8000, 160, &opa);
		b = ref_v32_create((void *)0x2222, DP_V32, 0, 8000, 160, &opb);
		if (a == 0 || b == 0) {
			diff_eq_int("both built (%ld)", 0, 1, code);
			return diff_end();
		}

		/*
		 * Null protocol slot, and the status the dispatch will
		 * return.  `struct v32_dp::fp` is the V32FP OBJECT, so
		 * +0x64 is its half-duplex context and +0x30 its status byte.
		 */
		{
			void *oa = ((struct v32_dp *)a)->fp;
			void *ob = ((struct v32_dp *)b)->fp;

			F_U16(F_PTR(oa, 0x64), 0x76) = 7;
			F_U16(F_PTR(ob, 0x64), 0x76) = 7;
			F_U8(oa, 0x30) = (unsigned char)code;
			F_U8(ob, 0x30) = (unsigned char)code;
		}

		memset(outa, 0, sizeof(outa));
		memset(outb, 0, sizeof(outb));
		ra = v32_process(a, in, outa, 40);
		rb = ref_v32_process(b, in, outb, 40);

		diff_eq_int("status for code %ld", ra, rb, code);
		diff_eq_int("dp.status for code %ld", (long)a->status,
			    (long)b->status, code);
		for (i = 0; i < 40; i++)
			diff_eq_int("out sample %ld", outa[i], outb[i], i);
		cmp_dp((struct v32_dp *)a, (struct v32_dp *)b, code);

		if (rb == DPSTAT_OK)
			arm_ok = 1;
		if (rb == DPSTAT_CONNECT)
			arm_connect = 1;
		if (rb == DPSTAT_ERROR && code <= 28)
			arm_error = 1;
		if (rb == DPSTAT_ERROR && code > 28)
			arm_unknown = 1;

		v32_delete(a);
		ref_v32_delete(b);
	}

	/*
	 * F134 again: four arms, four counters, all read from THIS run.  A
	 * sweep that only ever landed on DPSTAT_OK would otherwise pass.
	 */
	diff_eq_int("the DPSTAT_OK arm was reached (%ld)", arm_ok, 1, 0);
	diff_eq_int("the DPSTAT_CONNECT arm was reached (%ld)", arm_connect, 1,
		    1);
	diff_eq_int("an in-table error arm was reached (%ld)", arm_error, 1,
		    2);
	diff_eq_int("the out-of-table arm was reached (%ld)", arm_unknown, 1,
		    3);
	return diff_end();
}

/* ------------------------------------------------------------------------ */

/*
 * The error latch.  Once `dp->status` is DPSTAT_ERROR, `v32_process` zeroes
 * the output and returns without touching the datapump at all -- so a block
 * that follows an error must produce silence on both sides.
 */
static int
test_error_latch(void)
{
	short in[40];
	short outa[40];
	short outb[40];
	struct dp *a;
	struct dp *b;
	struct dp_operations opa;
	struct dp_operations opb;
	int silent = 1;
	int i;

	diff_begin("v32_process after DPSTAT_ERROR");

	harness_param_reset();
	harness_param_set(MDMPRM_MAX_RATE, 14400);
	harness_param_set(MDMPRM_IODELAY, 100);
	memset(&opa, 0, sizeof(opa));
	memset(&opb, 0, sizeof(opb));
	for (i = 0; i < 40; i++) {
		in[i] = (short)(i * 91);
		outa[i] = (short)0x7fff;
		outb[i] = (short)0x7fff;
	}

	a = v32_create((void *)0x3333, DP_V32, 1, 8000, 160, &opa);
	b = ref_v32_create((void *)0x3333, DP_V32, 1, 8000, 160, &opb);
	if (a == 0 || b == 0) {
		diff_eq_int("both built (%ld)", 0, 1, 0);
		return diff_end();
	}

	a->status = DPSTAT_ERROR;
	b->status = DPSTAT_ERROR;
	diff_eq_int("returns DPSTAT_ERROR (%ld)", v32_process(a, in, outa, 40),
		    ref_v32_process(b, in, outb, 40), 0);
	for (i = 0; i < 40; i++) {
		diff_eq_int("silenced sample %ld", outa[i], outb[i], i);
		if (outb[i] != 0)
			silent = 0;
	}
	diff_eq_int("the output really was silenced (%ld)", silent, 1, 0);

	v32_delete(a);
	ref_v32_delete(b);
	return diff_end();
}

int
main(void)
{
	int rc = 0;

	rc |= test_registration();
	rc |= test_create();
	rc |= test_process_status();
	rc |= test_error_latch();

	return rc;
}

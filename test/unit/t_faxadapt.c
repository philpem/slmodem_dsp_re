/*
 * t_faxadapt.c -- differential test of the 27 lowercase per-modulation
 * adapters written in `src/fax/faxadapt.c` (finding F9271): the 4 RX
 * creates, and the 8 deletes / 8 statuses / 7 processes across both sides.
 * The 8 message adapters are `class1tx.c`'s and are tested in
 * `t_class1leaves.c`, not here.
 *
 * ---------------------------------------------------------------------------
 * WHY THE RX SIDE AND THE TX SIDE ARE TESTED DIFFERENTLY
 *
 * The RX side is cheap and safe: `v??rx_create` is itself one of the
 * adapters under test, and once it is shown correct it is also the
 * legitimate way to build a REAL handle for the matching delete/status/
 * process adapter -- no guessing about internal layout required anywhere.
 *
 * The TX side has no create (all four are blocked -- see faxadapt.h), so
 * there is no legitimate way to obtain a real TX handle.  What this file
 * does instead is construct the SMALLEST fixture that will not crash the
 * already-reconstructed `V??TX_delete`/`V??TX_status`/`V??TX_modem` it
 * forwards to, built entirely from struct types and field offsets already
 * public in `v17fax.h`/`v17data.h`/`v21fax.h`/`v27fax.h`/`v29fax.h`/
 * `v29data.h` -- nothing here is guessed.  Reading those four `delete`
 * functions (and the `FPM_PPS_free`/`FPM_FSM_delete`/`FPM_MRF_free`/
 * `FPM_TONE_delete`/`SGD_delete`/`FIFO_delete` they call) establishes the
 * SAME rule everywhere: a pointer that gets DEREFERENCED DIRECTLY by the
 * callee must be a real, valid, big-enough address; a pointer that is only
 * ever handed to `sysdep_free` may be anything, including zero or garbage,
 * because the harness's allocator "swallows" an unrecognised free rather
 * than passing it to the real one (`harness.h`, `bad_free`).  So every
 * fixture below is zeroed EXCEPT the handful of slots that are themselves
 * pointers a callee dereferences -- those get the address of another real,
 * zeroed, correctly-typed object, one level deep.
 *
 * `V17TX_modem`/`V21TX_modem`/`V29TX_modem` additionally CALL THROUGH a
 * function pointer at `V??TXP_PROCESS` (the per-block "process" slot).  A
 * zeroed slot there is a call through NULL, so the TX process fixtures plant
 * a small PROBE function of the right type instead -- one C function, shared
 * by all three, that records the arguments it was called with and stops the
 * budget loop after one iteration.  This tests exactly what the adapter is
 * responsible for (which field goes to which formal parameter) without
 * needing `V??TX_modem`'s own DSP behaviour to be re-proven here; that is
 * already `t_v17fax.c`/`t_v21fax.c`/`t_v29fax.c`'s job.  `v27tx_process` is
 * the one BLOCKED member (V27TX_modem unwritten) and has no test here.
 *
 * `V??TX_OBJ_PARAMS`'s `INT_0008`/`INT_0004` selector is set to 1 in every
 * TX process fixture so `V??TX_modem` takes the "already taken" arm instead
 * of calling `FIFO_write` on a fabricated FIFO -- one fewer real object to
 * build, and it is the same choice `t_v17fax.c` is free to make in its own
 * fixtures for the same reason.
 *
 * D955/F8587: every `struct faxvmi_link` field OTHER than `int_0014` is
 * planted with a recognisable non-zero pattern before a TX delete/status/
 * process call and checked UNCHANGED afterward -- the object's own adapter
 * reads only `int_0014`, so a wrong-offset bug reads or writes elsewhere and
 * this catches it even though delete/status never touch `dp` on their own.
 */

#include <stddef.h>
#include <stdio.h>
#include <string.h>

#include "harness.h"

#include "dsplib/debug.h"
#include "dsplib/faxadapt.h"
#include "dsplib/faxcfg.h"
#include "dsplib/faxfifo.h"
#include "dsplib/faxvmi.h"
#include "dsplib/fpm_fsm.h"
#include "dsplib/fpm_mrf.h"
#include "dsplib/fpm_pps.h"
#include "dsplib/fpm_smc.h"
#include "dsplib/fpm_tone.h"
#include "dsplib/sgd.h"
#include "dsplib/v17data.h"
#include "dsplib/v17fax.h"
#include "dsplib/v21cfg.h"
#include "dsplib/v21fax.h"
#include "dsplib/v27fax.h"
#include "dsplib/v29data.h"
#include "dsplib/v29fax.h"

/* ------------------------------------------------------------------- */
/* The blob's side of the 27 adapters under test                        */

extern void ref_v17tx_create(struct faxvmi_link *dp,
			     const struct v17tx_cfg *cfg);
extern void ref_v17rx_create(struct faxvmi_link *dp,
			     const struct v17rx_cfg *cfg);
extern void ref_v21tx_create(struct faxvmi_link *dp,
			     const struct v21tx_cfg *cfg);
extern void ref_v21rx_create(struct faxvmi_link *dp,
			     const struct v21rx_cfg *cfg);
extern void ref_v27tx_create(struct faxvmi_link *dp,
			     const struct v27tx_cfg *cfg);
extern void ref_v27rx_create(struct faxvmi_link *dp,
			     const struct v27rx_cfg *cfg);
extern void ref_v29tx_create(struct faxvmi_link *dp,
			     const struct v29tx_cfg *cfg);
extern void ref_v29rx_create(struct faxvmi_link *dp,
			     const struct v29rx_cfg *cfg);

extern void ref_v17tx_delete(struct faxvmi_link *dp);
extern void ref_v17rx_delete(struct faxvmi_link *dp);
extern void ref_v21tx_delete(struct faxvmi_link *dp);
extern void ref_v21rx_delete(struct faxvmi_link *dp);
extern void ref_v27tx_delete(struct faxvmi_link *dp);
extern void ref_v27rx_delete(struct faxvmi_link *dp);
extern void ref_v29tx_delete(struct faxvmi_link *dp);
extern void ref_v29rx_delete(struct faxvmi_link *dp);

extern int ref_v17tx_status(struct faxvmi_link *dp, struct v17_status *st);
extern int ref_v17rx_status(struct faxvmi_link *dp, struct v17_status *st);
extern int ref_v21tx_status(struct faxvmi_link *dp, struct v21_status *st);
extern int ref_v21rx_status(struct faxvmi_link *dp, struct v21_status *st);
extern int ref_v27tx_status(struct faxvmi_link *dp, void *status);
extern int ref_v27rx_status(struct faxvmi_link *dp, void *status);
extern int ref_v29tx_status(struct faxvmi_link *dp, void *status);
extern int ref_v29rx_status(struct faxvmi_link *dp, void *status);

extern void ref_v17tx_process(struct faxvmi_link *dp, short *out,
			      unsigned short *count, unsigned short *result);
extern void ref_v17rx_process(struct faxvmi_link *dp, short *in,
			      unsigned short *result, unsigned short *count);
extern void ref_v21tx_process(struct faxvmi_link *dp, short *out,
			      unsigned short *count, unsigned short *result);
extern void ref_v21rx_process(struct faxvmi_link *dp, short *in,
			      unsigned short *result, unsigned short *count);
extern void ref_v27tx_process(struct faxvmi_link *dp, short *out,
			      unsigned short *count, unsigned short *result);
extern void ref_v27rx_process(struct faxvmi_link *dp, short *in,
			      unsigned short *result, unsigned short *count);
extern void ref_v29tx_process(struct faxvmi_link *dp, short *out,
			      unsigned short *count, unsigned short *result);
extern void ref_v29rx_process(struct faxvmi_link *dp, short *in,
			      unsigned short *result, unsigned short *count);

/* The five `*_control` adapters landed this wave (finding F10010). */
extern int ref_v17rx_control(struct faxvmi_link *dp,
			     const struct v17rx_ctl *arg);
extern int ref_v21tx_control(struct faxvmi_link *dp,
			     const struct v21tx_ctl *arg);
extern int ref_v21rx_control(struct faxvmi_link *dp,
			     const struct v21rx_ctl *arg);
extern int ref_v27tx_control(struct faxvmi_link *dp, void *req);
extern int ref_v27rx_control(struct faxvmi_link *dp, void *req);

/*
 * The last three `*_control` adapters, unblocked by `V17TX_control`/
 * `V29TX_control` landing (finding F10107).  `v29rx_control` was ALSO
 * blocked in this file's own comments even though `V29RX_control` itself
 * had already landed in wave 9 (F10103) -- the same stale-comment shape
 * F10107 corrected in `faxadapt.h`.
 */
extern int ref_v17tx_control(struct faxvmi_link *dp,
			     const struct v17tx_control_req *arg);
extern int ref_v29tx_control(struct faxvmi_link *dp,
			     const struct v29tx_control_req *arg);
extern int ref_v29rx_control(struct faxvmi_link *dp,
			     const struct v29rx_control_req *arg);

/* The underlying constructors/destructors, both sides, for RX handles. */
extern void *ref_V17RX_create(void *modem, const struct v17rx_cfg *params);
extern void *ref_V21RX_create(void *modem, const struct v21rx_cfg *params);
extern void *ref_V27RX_create(void *modem, const struct v27rx_cfg *cfg);
extern void *ref_V29RX_create(void *modem, const struct v29rx_cfg *params);
extern void ref_V17RX_delete(void *modem);
extern void ref_V21RX_delete(void *modem);
extern void ref_V27RX_delete(void *modem);
extern void ref_V29RX_delete(void *modem);

/* V.17's TX side has a real create since F9910; V.21/V.27/V.29's since
 * F10010 (this wave).  `V??TX_delete` is already declared through each
 * modulation's own header. */
extern void ref_V17TX_delete(void *modem);
extern void ref_V21TX_delete(void *modem);
extern void ref_V27TX_delete(void *modem);
extern void ref_V29TX_delete(void *modem);

/* ------------------------------------------------------------------- */
/* Small helpers                                                        */

static void
put_ptr(unsigned char *p, int off, void *v)
{
	memcpy(p + off, &v, sizeof v);
}

static void
put_i(unsigned char *p, int off, int v)
{
	memcpy(p + off, &v, sizeof v);
}

/* Poison every `struct faxvmi_link` field except `int_0014`. */
static void
poison_link(struct faxvmi_link *l, unsigned tag)
{
	memset(l, 0, sizeof *l);
	l->ptr_0000 = (unsigned short *)(void *)(long)(0x10000000u + tag);
	l->buf = (unsigned short *)(void *)(long)(0x20000000u + tag);
	l->pack_count = (short)(0x1100 + (short)tag);
	l->pack_width = (unsigned short)(0x2200 + tag);
	l->unpack_width = (unsigned short)(0x3300 + tag);
}

static long
link_canary_diff(const struct faxvmi_link *a, const struct faxvmi_link *b)
{
	if (a->ptr_0000 != b->ptr_0000)
		return 1;
	if (a->buf != b->buf)
		return 2;
	if (a->pack_count != b->pack_count)
		return 3;
	if (a->pack_width != b->pack_width)
		return 4;
	if (a->unpack_width != b->unpack_width)
		return 5;
	return 0;
}

/* ------------------------------------------------------------------- */
/* RX create: the one genuinely stateful adapter under test             */

struct rx_case {
	const char *name;
	int use_default;
	short bit_rate;
};

static const struct rx_case v17_cases[] = {
	{ "default (14400)", 1, 14400 },
	{ "14400", 0, 14400 },
	{ "12000", 0, 12000 },
	{ "9600", 0, 9600 },
	{ "7200", 0, 7200 },
	{ "unrecognised", 0, 4800 },
};

static const struct rx_case v21_cases[] = {
	{ "default (300)", 1, 300 },
	{ "300", 0, 300 },
};

static const struct rx_case v27_cases[] = {
	{ "default (4800)", 1, 4800 },
	{ "2400", 0, 2400 },
	{ "4800", 0, 4800 },
};

static const struct rx_case v29_cases[] = {
	{ "default (9600)", 1, 9600 },
	{ "9600", 0, 9600 },
	{ "7200", 0, 7200 },
};

static int
run_rx_create(void)
{
	long k;
	char buf[128];

	diff_begin("v17rx_create/v21rx_create/v27rx_create/v29rx_create");

	for (k = 0; k < (long)(sizeof(v17_cases) / sizeof(v17_cases[0])); k++) {
		struct v17rx_cfg c;
		struct faxvmi_link la, lb;

		c = V17RX_CFG;
		c.bit_rate = v17_cases[k].bit_rate;
		poison_link(&la, 0x10);
		poison_link(&lb, 0x10);

		v17rx_create(&la, v17_cases[k].use_default ? NULL : &c);
		ref_v17rx_create(&lb, v17_cases[k].use_default ? NULL : &c);

		snprintf(buf, sizeof(buf), "v17rx_create %s: pack_count (%%ld)",
			 v17_cases[k].name);
		diff_eq_int(buf, la.pack_count, lb.pack_count, k);
		snprintf(buf, sizeof(buf), "v17rx_create %s: pack_width (%%ld)",
			 v17_cases[k].name);
		diff_eq_int(buf, la.pack_width, lb.pack_width, k);
		snprintf(buf, sizeof(buf), "v17rx_create %s: unpack_width (%%ld)",
			 v17_cases[k].name);
		diff_eq_int(buf, la.unpack_width, lb.unpack_width, k);
		snprintf(buf, sizeof(buf), "v17rx_create %s: handle set (%%ld)",
			 v17_cases[k].name);
		diff_eq_int(buf, la.int_0014 != 0, lb.int_0014 != 0, k);

		V17RX_delete((void *)(long)la.int_0014);
		ref_V17RX_delete((void *)(long)lb.int_0014);
	}

	for (k = 0; k < (long)(sizeof(v21_cases) / sizeof(v21_cases[0])); k++) {
		struct v21rx_cfg c;
		struct faxvmi_link la, lb;

		c = V21RX_CFG;
		c.bit_rate = v21_cases[k].bit_rate;
		poison_link(&la, 0x20);
		poison_link(&lb, 0x20);

		v21rx_create(&la, v21_cases[k].use_default ? NULL : &c);
		ref_v21rx_create(&lb, v21_cases[k].use_default ? NULL : &c);

		snprintf(buf, sizeof(buf), "v21rx_create %s: pack_count (%%ld)",
			 v21_cases[k].name);
		diff_eq_int(buf, la.pack_count, lb.pack_count, k);
		snprintf(buf, sizeof(buf), "v21rx_create %s: pack_width (%%ld)",
			 v21_cases[k].name);
		diff_eq_int(buf, la.pack_width, lb.pack_width, k);
		snprintf(buf, sizeof(buf), "v21rx_create %s: unpack_width (%%ld)",
			 v21_cases[k].name);
		diff_eq_int(buf, la.unpack_width, lb.unpack_width, k);

		V21RX_delete((void *)(long)la.int_0014);
		ref_V21RX_delete((void *)(long)lb.int_0014);
	}

	for (k = 0; k < (long)(sizeof(v27_cases) / sizeof(v27_cases[0])); k++) {
		struct v27rx_cfg c;
		struct faxvmi_link la, lb;

		c = V27RX_CFG;
		c.bit_rate = v27_cases[k].bit_rate;
		poison_link(&la, 0x30);
		poison_link(&lb, 0x30);

		v27rx_create(&la, v27_cases[k].use_default ? NULL : &c);
		ref_v27rx_create(&lb, v27_cases[k].use_default ? NULL : &c);

		snprintf(buf, sizeof(buf), "v27rx_create %s: pack_count (%%ld)",
			 v27_cases[k].name);
		diff_eq_int(buf, la.pack_count, lb.pack_count, k);
		snprintf(buf, sizeof(buf), "v27rx_create %s: pack_width (%%ld)",
			 v27_cases[k].name);
		diff_eq_int(buf, la.pack_width, lb.pack_width, k);
		snprintf(buf, sizeof(buf), "v27rx_create %s: unpack_width (%%ld)",
			 v27_cases[k].name);
		diff_eq_int(buf, la.unpack_width, lb.unpack_width, k);

		V27RX_delete((void *)(long)la.int_0014);
		ref_V27RX_delete((void *)(long)lb.int_0014);
	}

	for (k = 0; k < (long)(sizeof(v29_cases) / sizeof(v29_cases[0])); k++) {
		struct v29rx_cfg c;
		struct faxvmi_link la, lb;

		c = V29RX_CFG;
		c.bit_rate = v29_cases[k].bit_rate;
		poison_link(&la, 0x40);
		poison_link(&lb, 0x40);

		v29rx_create(&la, v29_cases[k].use_default ? NULL : &c);
		ref_v29rx_create(&lb, v29_cases[k].use_default ? NULL : &c);

		snprintf(buf, sizeof(buf), "v29rx_create %s: pack_count (%%ld)",
			 v29_cases[k].name);
		diff_eq_int(buf, la.pack_count, lb.pack_count, k);
		snprintf(buf, sizeof(buf), "v29rx_create %s: pack_width (%%ld)",
			 v29_cases[k].name);
		diff_eq_int(buf, la.pack_width, lb.pack_width, k);
		snprintf(buf, sizeof(buf), "v29rx_create %s: unpack_width (%%ld)",
			 v29_cases[k].name);
		diff_eq_int(buf, la.unpack_width, lb.unpack_width, k);

		V29RX_delete((void *)(long)la.int_0014);
		ref_V29RX_delete((void *)(long)lb.int_0014);
	}

	return diff_end();
}

/*
 * `v17tx_create` -- the one TX-side adapter with a real constructor behind
 * it (finding F9910; the other three TX creates are still BLOCKED, see
 * faxadapt.h).  Reuses `struct rx_case` even though the field is a bit
 * rate rather than a "RX" anything; the shape is identical.
 */
static const struct rx_case v17tx_cases[] = {
	{ "default (14400)", 1, 14400 },
	{ "14400", 0, 14400 },
	{ "12000", 0, 12000 },
	{ "9600", 0, 9600 },
	{ "7200", 0, 7200 },
	{ "unrecognised", 0, 4800 },
};

static int
run_tx_create_v17(void)
{
	long k;
	char buf[128];

	diff_begin("v17tx_create");

	for (k = 0; k < (long)(sizeof(v17tx_cases) / sizeof(v17tx_cases[0]));
	     k++) {
		struct v17tx_cfg c;
		struct faxvmi_link la, lb;

		c = V17TX_CFG;
		c.bitrate = v17tx_cases[k].bit_rate;
		poison_link(&la, 0x70);
		poison_link(&lb, 0x70);

		v17tx_create(&la, v17tx_cases[k].use_default ? NULL : &c);
		ref_v17tx_create(&lb, v17tx_cases[k].use_default ? NULL : &c);

		snprintf(buf, sizeof(buf), "v17tx_create %s: pack_count (%%ld)",
			 v17tx_cases[k].name);
		diff_eq_int(buf, la.pack_count, lb.pack_count, k);
		snprintf(buf, sizeof(buf), "v17tx_create %s: pack_count is 0x30 (%%ld)",
			 v17tx_cases[k].name);
		diff_eq_int(buf, la.pack_count, 0x30, k);
		snprintf(buf, sizeof(buf), "v17tx_create %s: pack_width (%%ld)",
			 v17tx_cases[k].name);
		diff_eq_int(buf, la.pack_width, lb.pack_width, k);
		snprintf(buf, sizeof(buf), "v17tx_create %s: unpack_width (%%ld)",
			 v17tx_cases[k].name);
		diff_eq_int(buf, la.unpack_width, lb.unpack_width, k);
		snprintf(buf, sizeof(buf), "v17tx_create %s: unpack_width is 0 (%%ld)",
			 v17tx_cases[k].name);
		diff_eq_int(buf, la.unpack_width, 0, k);
		snprintf(buf, sizeof(buf), "v17tx_create %s: handle set (%%ld)",
			 v17tx_cases[k].name);
		diff_eq_int(buf, la.int_0014 != 0, lb.int_0014 != 0, k);

		V17TX_delete((void *)(long)la.int_0014);
		ref_V17TX_delete((void *)(long)lb.int_0014);
	}

	return diff_end();
}

/*
 * `v21tx_create`/`v27tx_create`/`v29tx_create` -- the three remaining TX
 * creates, landed this wave (finding F10010) now that `V21TX_create`,
 * `V27TX_create` and `V29TX_create` are all written.  Same shape as
 * `run_tx_create_v17` above: build with/without a caller `cfg`, across every
 * rate branch each `*tx_create` distinguishes, and free with the matching
 * `V??TX_delete` afterward.
 */
static const struct rx_case v21tx_cases[] = {
	{ "default (300)", 1, 300 },
	{ "300", 0, 300 },
};

static const struct rx_case v27tx_cases[] = {
	{ "default (4800)", 1, 4800 },
	{ "2400", 0, 2400 },
	{ "4800", 0, 4800 },
};

static const struct rx_case v29tx_cases[] = {
	{ "default (9600)", 1, 9600 },
	{ "9600", 0, 9600 },
	{ "7200", 0, 7200 },
};

static int
run_tx_create_v21(void)
{
	long k;
	char buf[128];

	diff_begin("v21tx_create");

	for (k = 0; k < (long)(sizeof(v21tx_cases) / sizeof(v21tx_cases[0]));
	     k++) {
		struct v21tx_cfg c;
		struct faxvmi_link la, lb;

		c = V21TX_CFG;
		poison_link(&la, 0x71);
		poison_link(&lb, 0x71);

		v21tx_create(&la, v21tx_cases[k].use_default ? NULL : &c);
		ref_v21tx_create(&lb, v21tx_cases[k].use_default ? NULL : &c);

		snprintf(buf, sizeof(buf), "v21tx_create %s: pack_count (%%ld)",
			 v21tx_cases[k].name);
		diff_eq_int(buf, la.pack_count, lb.pack_count, k);
		snprintf(buf, sizeof(buf), "v21tx_create %s: pack_count is 6 (%%ld)",
			 v21tx_cases[k].name);
		diff_eq_int(buf, la.pack_count, 6, k);
		snprintf(buf, sizeof(buf), "v21tx_create %s: pack_width (%%ld)",
			 v21tx_cases[k].name);
		diff_eq_int(buf, la.pack_width, lb.pack_width, k);
		snprintf(buf, sizeof(buf), "v21tx_create %s: pack_width is 1 (%%ld)",
			 v21tx_cases[k].name);
		diff_eq_int(buf, la.pack_width, 1, k);
		snprintf(buf, sizeof(buf), "v21tx_create %s: unpack_width (%%ld)",
			 v21tx_cases[k].name);
		diff_eq_int(buf, la.unpack_width, lb.unpack_width, k);
		snprintf(buf, sizeof(buf), "v21tx_create %s: handle set (%%ld)",
			 v21tx_cases[k].name);
		diff_eq_int(buf, la.int_0014 != 0, lb.int_0014 != 0, k);

		V21TX_delete((void *)(long)la.int_0014);
		ref_V21TX_delete((void *)(long)lb.int_0014);
	}

	return diff_end();
}

static int
run_tx_create_v27(void)
{
	long k;
	char buf[128];

	diff_begin("v27tx_create");

	for (k = 0; k < (long)(sizeof(v27tx_cases) / sizeof(v27tx_cases[0]));
	     k++) {
		struct v27tx_cfg c;
		struct faxvmi_link la, lb;

		c = V27TX_CFG;
		c.bitrate = v27tx_cases[k].bit_rate;
		poison_link(&la, 0x72);
		poison_link(&lb, 0x72);

		v27tx_create(&la, v27tx_cases[k].use_default ? NULL : &c);
		ref_v27tx_create(&lb, v27tx_cases[k].use_default ? NULL : &c);

		snprintf(buf, sizeof(buf), "v27tx_create %s: pack_count (%%ld)",
			 v27tx_cases[k].name);
		diff_eq_int(buf, la.pack_count, lb.pack_count, k);
		snprintf(buf, sizeof(buf), "v27tx_create %s: pack_width (%%ld)",
			 v27tx_cases[k].name);
		diff_eq_int(buf, la.pack_width, lb.pack_width, k);
		snprintf(buf, sizeof(buf), "v27tx_create %s: unpack_width (%%ld)",
			 v27tx_cases[k].name);
		diff_eq_int(buf, la.unpack_width, lb.unpack_width, k);
		snprintf(buf, sizeof(buf), "v27tx_create %s: unpack_width is 0 (%%ld)",
			 v27tx_cases[k].name);
		diff_eq_int(buf, la.unpack_width, 0, k);
		snprintf(buf, sizeof(buf), "v27tx_create %s: handle set (%%ld)",
			 v27tx_cases[k].name);
		diff_eq_int(buf, la.int_0014 != 0, lb.int_0014 != 0, k);

		V27TX_delete((void *)(long)la.int_0014);
		ref_V27TX_delete((void *)(long)lb.int_0014);
	}

	return diff_end();
}

static int
run_tx_create_v29(void)
{
	long k;
	char buf[128];

	diff_begin("v29tx_create");

	for (k = 0; k < (long)(sizeof(v29tx_cases) / sizeof(v29tx_cases[0]));
	     k++) {
		struct v29tx_cfg c;
		struct faxvmi_link la, lb;

		c = V29TX_CFG;
		c.bitrate = v29tx_cases[k].bit_rate;
		poison_link(&la, 0x73);
		poison_link(&lb, 0x73);

		v29tx_create(&la, v29tx_cases[k].use_default ? NULL : &c);
		ref_v29tx_create(&lb, v29tx_cases[k].use_default ? NULL : &c);

		snprintf(buf, sizeof(buf), "v29tx_create %s: pack_count (%%ld)",
			 v29tx_cases[k].name);
		diff_eq_int(buf, la.pack_count, lb.pack_count, k);
		snprintf(buf, sizeof(buf), "v29tx_create %s: pack_count is 0x30 (%%ld)",
			 v29tx_cases[k].name);
		diff_eq_int(buf, la.pack_count, 0x30, k);
		snprintf(buf, sizeof(buf), "v29tx_create %s: pack_width (%%ld)",
			 v29tx_cases[k].name);
		diff_eq_int(buf, la.pack_width, lb.pack_width, k);
		snprintf(buf, sizeof(buf), "v29tx_create %s: unpack_width (%%ld)",
			 v29tx_cases[k].name);
		diff_eq_int(buf, la.unpack_width, lb.unpack_width, k);
		snprintf(buf, sizeof(buf), "v29tx_create %s: unpack_width is 0 (%%ld)",
			 v29tx_cases[k].name);
		diff_eq_int(buf, la.unpack_width, 0, k);
		snprintf(buf, sizeof(buf), "v29tx_create %s: handle set (%%ld)",
			 v29tx_cases[k].name);
		diff_eq_int(buf, la.int_0014 != 0, lb.int_0014 != 0, k);

		V29TX_delete((void *)(long)la.int_0014);
		ref_V29TX_delete((void *)(long)lb.int_0014);
	}

	return diff_end();
}

/* ------------------------------------------------------------------- */
/* RX delete/status/process: driven off a real, freshly-created handle  */

static int
run_rx_delete(void)
{
	long tag = 0;

	diff_begin("v17rx_delete/v21rx_delete/v27rx_delete/v29rx_delete");

#define RX_DELETE_CASE(mod, create_fn, ref_create_fn, delete_fn, \
			ref_delete_fn, cfg_ty, cfg_default)	\
	do {							\
		cfg_ty c = (cfg_default);			\
		struct faxvmi_link la, lb;			\
		void *ha, *hb;					\
		poison_link(&la, 0x50);			\
		poison_link(&lb, 0x50);				\
		create_fn(&la, &c);				\
		ref_create_fn(&lb, &c);				\
		ha = (void *)(long)la.int_0014;		\
		hb = (void *)(long)lb.int_0014;		\
		diff_eq_int(mod " delete: live before, ours (%ld)",	\
			    harness_alloc_ordinal(ha) != 0, 1, tag);	\
		diff_eq_int(mod " delete: live before, ref (%ld)",	\
			    harness_alloc_ordinal(hb) != 0, 1, tag);	\
		delete_fn(&la);					\
		ref_delete_fn(&lb);				\
		diff_eq_int(mod " delete: freed, ours (%ld)",		\
			    harness_alloc_ordinal(ha) == 0, 1, tag);	\
		diff_eq_int(mod " delete: freed, ref (%ld)",		\
			    harness_alloc_ordinal(hb) == 0, 1, tag);	\
		tag++;						\
	} while (0)

	RX_DELETE_CASE("v17rx", v17rx_create, ref_v17rx_create,
			v17rx_delete, ref_v17rx_delete,
			struct v17rx_cfg, V17RX_CFG);
	RX_DELETE_CASE("v21rx", v21rx_create, ref_v21rx_create,
			v21rx_delete, ref_v21rx_delete,
			struct v21rx_cfg, V21RX_CFG);
	RX_DELETE_CASE("v27rx", v27rx_create, ref_v27rx_create,
			v27rx_delete, ref_v27rx_delete,
			struct v27rx_cfg, V27RX_CFG);
	RX_DELETE_CASE("v29rx", v29rx_create, ref_v29rx_create,
			v29rx_delete, ref_v29rx_delete,
			struct v29rx_cfg, V29RX_CFG);

#undef RX_DELETE_CASE

	return diff_end();
}

static int
run_rx_status(void)
{
	long tag = 0;
	struct faxvmi_link la, lb;
	struct v17rx_cfg c17;
	struct v21rx_cfg c21;
	struct v27rx_cfg c27;
	struct v29rx_cfg c29;
	struct v17_status s17a, s17b;
	struct v21_status s21a, s21b;
	unsigned char s27a[64], s27b[64];
	unsigned char s29a[64], s29b[64];

	diff_begin("v17rx_status/v21rx_status/v27rx_status/v29rx_status");

	c17 = V17RX_CFG;
	poison_link(&la, 0x60);
	poison_link(&lb, 0x60);
	v17rx_create(&la, &c17);
	ref_v17rx_create(&lb, &c17);
	memset(&s17a, 0xa5, sizeof s17a);
	memset(&s17b, 0xa5, sizeof s17b);
	v17rx_status(&la, &s17a);
	ref_v17rx_status(&lb, &s17b);
	diff_eq_int("v17rx_status: struct byte-identical (%ld)",
		    memcmp(&s17a, &s17b, sizeof s17a), 0, tag);
	V17RX_delete((void *)(long)la.int_0014);
	ref_V17RX_delete((void *)(long)lb.int_0014);
	tag++;

	c21 = V21RX_CFG;
	poison_link(&la, 0x60);
	poison_link(&lb, 0x60);
	v21rx_create(&la, &c21);
	ref_v21rx_create(&lb, &c21);
	memset(&s21a, 0xa5, sizeof s21a);
	memset(&s21b, 0xa5, sizeof s21b);
	v21rx_status(&la, &s21a);
	ref_v21rx_status(&lb, &s21b);
	diff_eq_int("v21rx_status: struct byte-identical (%ld)",
		    memcmp(&s21a, &s21b, sizeof s21a), 0, tag);
	V21RX_delete((void *)(long)la.int_0014);
	ref_V21RX_delete((void *)(long)lb.int_0014);
	tag++;

	c27 = V27RX_CFG;
	poison_link(&la, 0x60);
	poison_link(&lb, 0x60);
	v27rx_create(&la, &c27);
	ref_v27rx_create(&lb, &c27);
	memset(s27a, 0xa5, sizeof s27a);
	memset(s27b, 0xa5, sizeof s27b);
	v27rx_status(&la, s27a);
	ref_v27rx_status(&lb, s27b);
	diff_eq_int("v27rx_status: 64-byte region byte-identical (%ld)",
		    memcmp(s27a, s27b, sizeof s27a), 0, tag);
	V27RX_delete((void *)(long)la.int_0014);
	ref_V27RX_delete((void *)(long)lb.int_0014);
	tag++;

	c29 = V29RX_CFG;
	poison_link(&la, 0x60);
	poison_link(&lb, 0x60);
	v29rx_create(&la, &c29);
	ref_v29rx_create(&lb, &c29);
	memset(s29a, 0xa5, sizeof s29a);
	memset(s29b, 0xa5, sizeof s29b);
	v29rx_status(&la, s29a);
	ref_v29rx_status(&lb, s29b);
	diff_eq_int("v29rx_status: 64-byte region byte-identical (%ld)",
		    memcmp(s29a, s29b, sizeof s29a), 0, tag);
	V29RX_delete((void *)(long)la.int_0014);
	ref_V29RX_delete((void *)(long)lb.int_0014);
	tag++;

	return diff_end();
}

static int
run_rx_process(void)
{
	long tag = 0;

	{
		struct v17rx_cfg c = V17RX_CFG;
		struct faxvmi_link la, lb;
		short in_a[16], in_b[16];
		unsigned short out_a[64], out_b[64];
		unsigned short result_a, result_b, count_a, count_b;
		int i;

		diff_begin("v17rx_process");
		poison_link(&la, 0x70);
		poison_link(&lb, 0x70);
		v17rx_create(&la, &c);
		ref_v17rx_create(&lb, &c);
		la.buf = out_a;
		lb.buf = out_b;
		for (i = 0; i < 16; i++)
			in_a[i] = in_b[i] = (short)(1000 + 37 * i);
		memset(out_a, 0x5a, sizeof out_a);
		memset(out_b, 0x5a, sizeof out_b);
		count_a = count_b = 16;
		result_a = result_b = 0xeeee;
		v17rx_process(&la, in_a, &result_a, &count_a);
		ref_v17rx_process(&lb, in_b, &result_b, &count_b);
		diff_eq_int("v17rx_process: result (%ld)", result_a,
			    result_b, tag);
		diff_eq_int("v17rx_process: count cleared, ours (%ld)",
			    count_a, 0, tag);
		diff_eq_int("v17rx_process: count cleared, ref (%ld)",
			    count_b, 0, tag);
		diff_eq_int("v17rx_process: out buffer identical (%ld)",
			    memcmp(out_a, out_b, sizeof out_a), 0, tag);
		V17RX_delete((void *)(long)la.int_0014);
		ref_V17RX_delete((void *)(long)lb.int_0014);
		tag += diff_end();
	}

	{
		struct v21rx_cfg c = V21RX_CFG;
		struct faxvmi_link la, lb;
		short in_a[16], in_b[16];
		unsigned short out_a[64], out_b[64];
		unsigned short result_a, result_b, count_a, count_b;
		int i;

		diff_begin("v21rx_process");
		poison_link(&la, 0x70);
		poison_link(&lb, 0x70);
		v21rx_create(&la, &c);
		ref_v21rx_create(&lb, &c);
		la.buf = out_a;
		lb.buf = out_b;
		for (i = 0; i < 16; i++)
			in_a[i] = in_b[i] = (short)(1000 + 37 * i);
		memset(out_a, 0x5a, sizeof out_a);
		memset(out_b, 0x5a, sizeof out_b);
		count_a = count_b = 16;
		result_a = result_b = 0xeeee;
		v21rx_process(&la, in_a, &result_a, &count_a);
		ref_v21rx_process(&lb, in_b, &result_b, &count_b);
		diff_eq_int("v21rx_process: result (%ld)", result_a,
			    result_b, tag);
		diff_eq_int("v21rx_process: count cleared, ours (%ld)",
			    count_a, 0, tag);
		diff_eq_int("v21rx_process: count cleared, ref (%ld)",
			    count_b, 0, tag);
		diff_eq_int("v21rx_process: out buffer identical (%ld)",
			    memcmp(out_a, out_b, sizeof out_a), 0, tag);
		V21RX_delete((void *)(long)la.int_0014);
		ref_V21RX_delete((void *)(long)lb.int_0014);
		tag += diff_end();
	}

	{
		struct v27rx_cfg c = V27RX_CFG;
		struct faxvmi_link la, lb;
		short in_a[16], in_b[16];
		unsigned short out_a[64], out_b[64];
		unsigned short result_a, result_b, count_a, count_b;
		int i;

		diff_begin("v27rx_process");
		poison_link(&la, 0x70);
		poison_link(&lb, 0x70);
		v27rx_create(&la, &c);
		ref_v27rx_create(&lb, &c);
		la.buf = out_a;
		lb.buf = out_b;
		for (i = 0; i < 16; i++)
			in_a[i] = in_b[i] = (short)(1000 + 37 * i);
		memset(out_a, 0x5a, sizeof out_a);
		memset(out_b, 0x5a, sizeof out_b);
		count_a = count_b = 16;
		result_a = result_b = 0xeeee;
		v27rx_process(&la, in_a, &result_a, &count_a);
		ref_v27rx_process(&lb, in_b, &result_b, &count_b);
		diff_eq_int("v27rx_process: result (%ld)", result_a,
			    result_b, tag);
		diff_eq_int("v27rx_process: count cleared, ours (%ld)",
			    count_a, 0, tag);
		diff_eq_int("v27rx_process: count cleared, ref (%ld)",
			    count_b, 0, tag);
		diff_eq_int("v27rx_process: out buffer identical (%ld)",
			    memcmp(out_a, out_b, sizeof out_a), 0, tag);
		V27RX_delete((void *)(long)la.int_0014);
		ref_V27RX_delete((void *)(long)lb.int_0014);
		tag += diff_end();
	}

	{
		struct v29rx_cfg c = V29RX_CFG;
		struct faxvmi_link la, lb;
		short in_a[16], in_b[16];
		unsigned short out_a[64], out_b[64];
		unsigned short result_a, result_b, count_a, count_b;
		int i;

		diff_begin("v29rx_process");
		poison_link(&la, 0x70);
		poison_link(&lb, 0x70);
		v29rx_create(&la, &c);
		ref_v29rx_create(&lb, &c);
		la.buf = out_a;
		lb.buf = out_b;
		for (i = 0; i < 16; i++)
			in_a[i] = in_b[i] = (short)(1000 + 37 * i);
		memset(out_a, 0x5a, sizeof out_a);
		memset(out_b, 0x5a, sizeof out_b);
		count_a = count_b = 16;
		result_a = result_b = 0xeeee;
		v29rx_process(&la, in_a, &result_a, &count_a);
		ref_v29rx_process(&lb, in_b, &result_b, &count_b);
		diff_eq_int("v29rx_process: result (%ld)", result_a,
			    result_b, tag);
		diff_eq_int("v29rx_process: count cleared, ours (%ld)",
			    count_a, 0, tag);
		diff_eq_int("v29rx_process: count cleared, ref (%ld)",
			    count_b, 0, tag);
		diff_eq_int("v29rx_process: out buffer identical (%ld)",
			    memcmp(out_a, out_b, sizeof out_a), 0, tag);
		V29RX_delete((void *)(long)la.int_0014);
		ref_V29RX_delete((void *)(long)lb.int_0014);
		tag += diff_end();
	}

	return tag != 0;
}

/* ------------------------------------------------------------------- */
/* TX side: no create exists, so a minimal safe fixture stands in.      */
/* Every intermediate pointer that a callee DEREFERENCES DIRECTLY is a  */
/* real, zeroed, correctly-typed object; everything only ever handed to */
/* `sysdep_free` is left zero.  See the file header for the derivation. */

struct v17tx_fixture {
	unsigned char modem[64];
	unsigned char fp[256];
	unsigned char prm[32];
	struct sgd sgd;
	struct fax_fifo fifo;
};

static void
v17tx_fixture_build(struct v17tx_fixture *f)
{
	memset(f, 0, sizeof *f);
	put_ptr(f->modem, V17TX_OBJ_FP, f->fp);
	put_ptr(f->modem, V17TX_OBJ_PARAMS, f->prm);
	put_ptr(f->prm, V17TXP_SGD, &f->sgd);
	put_ptr(f->prm, V17TXP_FIFO, &f->fifo);
}

struct v21tx_fixture {
	unsigned char modem[64];
	struct v21_tx_dsp dsp;
	struct fpm_tone tone;
	unsigned char prm[32];
	struct fax_fifo fifo;
};

static void
v21tx_fixture_build(struct v21tx_fixture *f)
{
	memset(f, 0, sizeof *f);
	f->dsp.fsm.tone = &f->tone;	/* FPM_TONE_delete derefs unconditionally */
	put_ptr(f->modem, V21TX_OBJ_DSP, &f->dsp);
	put_ptr(f->modem, V21TX_OBJ_PARAMS, f->prm);
	put_ptr(f->prm, V21TXP_FIFO, &f->fifo);
}

struct v27tx_fixture {
	unsigned char modem[64];
	unsigned char tx[256];	/* V27TX_PPS (0x5c) + sizeof(struct fpm_pps) */
	unsigned char txdata[32];
	struct sgd sgd;
	struct fax_fifo fifo;
};

static void
v27tx_fixture_build(struct v27tx_fixture *f)
{
	memset(f, 0, sizeof *f);
	put_ptr(f->modem, V27_OBJ_TX, f->tx);
	put_ptr(f->modem, V27_OBJ_TXDATA, f->txdata);
	put_ptr(f->txdata, V27TXD_FIFO, &f->fifo);
	put_ptr(f->txdata, V27TXD_SGD, &f->sgd);
}

struct v29tx_fixture {
	unsigned char modem[64];
	unsigned char fp[256];
	unsigned char prm[32];
	struct sgd sgd;
	struct fax_fifo fifo;
};

static void
v29tx_fixture_build(struct v29tx_fixture *f)
{
	memset(f, 0, sizeof *f);
	put_ptr(f->modem, V29TX_OBJ_FP, f->fp);
	put_ptr(f->modem, V29TX_OBJ_PARAMS, f->prm);
	put_ptr(f->prm, V29TXP_SGD, &f->sgd);
	put_ptr(f->prm, V29TXP_FIFO, &f->fifo);
}

/*
 * NONE OF THE FOUR OBJECT-LEVEL `V??TX_delete` FUNCTIONS WRITE BACK INTO
 * `dp` -- the object's own adapter only ever reads `int_0014` and tail-jumps
 * -- so the two things worth asserting per call are (1) `dp` is byte-for-byte
 * unchanged (the D955 canary, catching a wrong-offset read or an accidental
 * write) and (2) the call returned at all: a wrong-offset bug that hands
 * `V??TX_delete` a non-pointer crashes the process, which `make one` reports
 * as the whole binary failing, so reaching `diff_end()` below is already the
 * survival half of the check.
 */
static int
run_tx_delete(void)
{
	long tag = 0;
	struct faxvmi_link la, lb, la0, lb0;

	diff_begin("v17tx_delete/v21tx_delete/v27tx_delete/v29tx_delete");

	{
		struct v17tx_fixture fa, fb;

		v17tx_fixture_build(&fa);
		v17tx_fixture_build(&fb);
		poison_link(&la, 0x80);
		poison_link(&lb, 0x80);
		la.int_0014 = (int)(long)fa.modem;
		lb.int_0014 = (int)(long)fb.modem;
		la0 = la;
		lb0 = lb;
		v17tx_delete(&la);
		ref_v17tx_delete(&lb);
		diff_eq_int("v17tx_delete: dp unchanged, ours (%ld)",
			    memcmp(&la, &la0, sizeof la), 0, tag);
		diff_eq_int("v17tx_delete: dp unchanged, ref (%ld)",
			    memcmp(&lb, &lb0, sizeof lb), 0, tag);
	}
	tag++;

	{
		struct v21tx_fixture fa, fb;

		v21tx_fixture_build(&fa);
		v21tx_fixture_build(&fb);
		poison_link(&la, 0x80);
		poison_link(&lb, 0x80);
		la.int_0014 = (int)(long)fa.modem;
		lb.int_0014 = (int)(long)fb.modem;
		la0 = la;
		lb0 = lb;
		v21tx_delete(&la);
		ref_v21tx_delete(&lb);
		diff_eq_int("v21tx_delete: dp unchanged, ours (%ld)",
			    memcmp(&la, &la0, sizeof la), 0, tag);
		diff_eq_int("v21tx_delete: dp unchanged, ref (%ld)",
			    memcmp(&lb, &lb0, sizeof lb), 0, tag);
	}
	tag++;

	{
		struct v27tx_fixture fa, fb;

		v27tx_fixture_build(&fa);
		v27tx_fixture_build(&fb);
		poison_link(&la, 0x80);
		poison_link(&lb, 0x80);
		la.int_0014 = (int)(long)fa.modem;
		lb.int_0014 = (int)(long)fb.modem;
		la0 = la;
		lb0 = lb;
		v27tx_delete(&la);
		ref_v27tx_delete(&lb);
		diff_eq_int("v27tx_delete: dp unchanged, ours (%ld)",
			    memcmp(&la, &la0, sizeof la), 0, tag);
		diff_eq_int("v27tx_delete: dp unchanged, ref (%ld)",
			    memcmp(&lb, &lb0, sizeof lb), 0, tag);
	}
	tag++;

	{
		struct v29tx_fixture fa, fb;

		v29tx_fixture_build(&fa);
		v29tx_fixture_build(&fb);
		poison_link(&la, 0x80);
		poison_link(&lb, 0x80);
		la.int_0014 = (int)(long)fa.modem;
		lb.int_0014 = (int)(long)fb.modem;
		la0 = la;
		lb0 = lb;
		v29tx_delete(&la);
		ref_v29tx_delete(&lb);
		diff_eq_int("v29tx_delete: dp unchanged, ours (%ld)",
			    memcmp(&la, &la0, sizeof la), 0, tag);
		diff_eq_int("v29tx_delete: dp unchanged, ref (%ld)",
			    memcmp(&lb, &lb0, sizeof lb), 0, tag);
	}
	tag++;

	return diff_end();
}

static int
run_tx_status(void)
{
	long tag = 0;
	struct faxvmi_link la, lb;

	diff_begin("v17tx_status/v21tx_status/v27tx_status/v29tx_status");

	{
		unsigned char pa[64], pb[64];
		struct v17_status sa, sb;
		int i;

		for (i = 0; i < (int)sizeof(pa); i++)
			pa[i] = pb[i] = (unsigned char)(i * 7 + 3);
		memset(&sa, 0x5a, sizeof sa);
		memset(&sb, 0x5a, sizeof sb);
		poison_link(&la, 0x90);
		poison_link(&lb, 0x90);
		la.int_0014 = (int)(long)pa;
		lb.int_0014 = (int)(long)pb;
		v17tx_status(&la, &sa);
		ref_v17tx_status(&lb, &sb);
		diff_eq_int("v17tx_status: struct byte-identical (%ld)",
			    memcmp(&sa, &sb, sizeof sa), 0, tag);
		diff_eq_int("v17tx_status: dp fields untouched (%ld)",
			    link_canary_diff(&la, &lb), 0, tag);
	}
	tag++;

	{
		unsigned char pa[64], pb[64];
		struct v21_status sa, sb;
		int i;

		for (i = 0; i < (int)sizeof(pa); i++)
			pa[i] = pb[i] = (unsigned char)(i * 11 + 5);
		memset(&sa, 0x5a, sizeof sa);
		memset(&sb, 0x5a, sizeof sb);
		poison_link(&la, 0x90);
		poison_link(&lb, 0x90);
		la.int_0014 = (int)(long)pa;
		lb.int_0014 = (int)(long)pb;
		v21tx_status(&la, &sa);
		ref_v21tx_status(&lb, &sb);
		diff_eq_int("v21tx_status: struct byte-identical (%ld)",
			    memcmp(&sa, &sb, sizeof sa), 0, tag);
		diff_eq_int("v21tx_status: dp fields untouched (%ld)",
			    link_canary_diff(&la, &lb), 0, tag);
	}
	tag++;

	{
		unsigned char pa[64], pb[64];
		unsigned char sa[64], sb[64];
		int i;

		for (i = 0; i < (int)sizeof(pa); i++)
			pa[i] = pb[i] = (unsigned char)(i * 13 + 9);
		memset(sa, 0x5a, sizeof sa);
		memset(sb, 0x5a, sizeof sb);
		poison_link(&la, 0x90);
		poison_link(&lb, 0x90);
		la.int_0014 = (int)(long)pa;
		lb.int_0014 = (int)(long)pb;
		v27tx_status(&la, sa);
		ref_v27tx_status(&lb, sb);
		diff_eq_int("v27tx_status: 64-byte region byte-identical (%ld)",
			    memcmp(sa, sb, sizeof sa), 0, tag);
		diff_eq_int("v27tx_status: dp fields untouched (%ld)",
			    link_canary_diff(&la, &lb), 0, tag);
	}
	tag++;

	{
		unsigned char pa[64], pb[64];
		unsigned char sa[64], sb[64];
		int i;

		for (i = 0; i < (int)sizeof(pa); i++)
			pa[i] = pb[i] = (unsigned char)(i * 17 + 1);
		memset(sa, 0x5a, sizeof sa);
		memset(sb, 0x5a, sizeof sb);
		poison_link(&la, 0x90);
		poison_link(&lb, 0x90);
		la.int_0014 = (int)(long)pa;
		lb.int_0014 = (int)(long)pb;
		v29tx_status(&la, sa);
		ref_v29tx_status(&lb, sb);
		diff_eq_int("v29tx_status: 64-byte region byte-identical (%ld)",
			    memcmp(sa, sb, sizeof sa), 0, tag);
		diff_eq_int("v29tx_status: dp fields untouched (%ld)",
			    link_canary_diff(&la, &lb), 0, tag);
	}
	tag++;

	return diff_end();
}

/* ------------------------------------------------------------------- */
/* TX process: a probe handler plugged into V??TXP_PROCESS              */

static void *probe_modem_seen;
static unsigned short *probe_in_seen;
static short *probe_out_seen;
static short probe_budget_seen;
static int probe_calls;

static short
probe_tx_handler(void *modem, unsigned short *in, short *out, short *budget)
{
	probe_modem_seen = modem;
	probe_in_seen = in;
	probe_out_seen = out;
	probe_budget_seen = *budget;
	*budget = 0;
	probe_calls++;
	return 0;
}

static int
run_tx_process(void)
{
	long tag = 0;

	diff_begin("v17tx_process/v21tx_process/v27tx_process/v29tx_process");

	{
		struct v17tx_fixture fa;
		struct faxvmi_link la;
		short out[16];
		unsigned short count, result;

		v17tx_fixture_build(&fa);
		put_i(fa.prm, V17TXP_INT_0008, 1);
		put_ptr(fa.prm, V17TXP_PROCESS, (void *)probe_tx_handler);
		poison_link(&la, 0xa0);
		la.buf = (unsigned short *)(void *)(fa.modem + 0);
		la.int_0014 = (int)(long)fa.modem;
		memset(out, 0x5a, sizeof out);
		count = 16;
		result = 0xeeee;
		probe_calls = 0;
		probe_modem_seen = 0;
		probe_in_seen = 0;
		probe_out_seen = 0;
		v17tx_process(&la, out, &count, &result);
		diff_eq_int("v17tx_process: probe called once (%ld)",
			    probe_calls, 1, tag);
		diff_eq_int("v17tx_process: modem forwarded (%ld)",
			    probe_modem_seen == (void *)fa.modem, 1, tag);
		diff_eq_int("v17tx_process: in is dp->buf (%ld)",
			    probe_in_seen == la.buf, 1, tag);
		diff_eq_int("v17tx_process: out is the caller's out (%ld)",
			    probe_out_seen == out, 1, tag);
		diff_eq_int("v17tx_process: result got the sample count (%ld)",
			    result, 0, tag);
		diff_eq_int("v17tx_process: count cleared (%ld)", count, 0,
			    tag);
	}
	tag++;

	{
		struct v21tx_fixture fa;
		struct faxvmi_link la;
		short out[16];
		unsigned short count, result;

		v21tx_fixture_build(&fa);
		put_i(fa.prm, V21TXP_INT_0004, 1);
		put_ptr(fa.prm, V21TXP_PROCESS, (void *)probe_tx_handler);
		poison_link(&la, 0xa0);
		la.buf = (unsigned short *)(void *)(fa.modem + 0);
		la.int_0014 = (int)(long)fa.modem;
		memset(out, 0x5a, sizeof out);
		count = 16;
		result = 0xeeee;
		probe_calls = 0;
		probe_modem_seen = 0;
		probe_in_seen = 0;
		probe_out_seen = 0;
		v21tx_process(&la, out, &count, &result);
		diff_eq_int("v21tx_process: probe called once (%ld)",
			    probe_calls, 1, tag);
		diff_eq_int("v21tx_process: modem forwarded (%ld)",
			    probe_modem_seen == (void *)fa.modem, 1, tag);
		diff_eq_int("v21tx_process: in is dp->buf (%ld)",
			    probe_in_seen == la.buf, 1, tag);
		diff_eq_int("v21tx_process: out is the caller's out (%ld)",
			    probe_out_seen == out, 1, tag);
		diff_eq_int("v21tx_process: result got the sample count (%ld)",
			    result, 0, tag);
		diff_eq_int("v21tx_process: count cleared (%ld)", count, 0,
			    tag);
	}
	tag++;

	{
		struct v29tx_fixture fa;
		struct faxvmi_link la;
		short out[16];
		unsigned short count, result;

		v29tx_fixture_build(&fa);
		put_i(fa.prm, V29TXP_INT_0008, 1);
		put_ptr(fa.prm, V29TXP_PROCESS, (void *)probe_tx_handler);
		poison_link(&la, 0xa0);
		la.buf = (unsigned short *)(void *)(fa.modem + 0);
		la.int_0014 = (int)(long)fa.modem;
		memset(out, 0x5a, sizeof out);
		count = 16;
		result = 0xeeee;
		probe_calls = 0;
		probe_modem_seen = 0;
		probe_in_seen = 0;
		probe_out_seen = 0;
		v29tx_process(&la, out, &count, &result);
		diff_eq_int("v29tx_process: probe called once (%ld)",
			    probe_calls, 1, tag);
		diff_eq_int("v29tx_process: modem forwarded (%ld)",
			    probe_modem_seen == (void *)fa.modem, 1, tag);
		diff_eq_int("v29tx_process: in is dp->buf (%ld)",
			    probe_in_seen == la.buf, 1, tag);
		diff_eq_int("v29tx_process: out is the caller's out (%ld)",
			    probe_out_seen == out, 1, tag);
		diff_eq_int("v29tx_process: result got the sample count (%ld)",
			    result, 0, tag);
		diff_eq_int("v29tx_process: count cleared (%ld)", count, 0,
			    tag);
	}
	tag++;

	/*
	 * v27tx_process, landed this wave (finding F10010) now that
	 * `V27TX_modem` is written.  Same probe, but through
	 * `V27TXP_INT_0008`/`V27TXP_PROCESS` at V27_OBJ_TXDATA rather than
	 * `V??TXP_...` on the modem's own `prm` block -- see faxadapt.c's
	 * comment and v27fax.h's V27TX_modem for the field's real home.
	 */
	{
		struct v27tx_fixture fa;
		struct faxvmi_link la;
		short out[16];
		unsigned short count, result;

		v27tx_fixture_build(&fa);
		put_i(fa.txdata, V27TXP_INT_0008, 1);
		put_ptr(fa.txdata, V27TXP_PROCESS, (void *)probe_tx_handler);
		poison_link(&la, 0xa0);
		la.buf = (unsigned short *)(void *)(fa.modem + 0);
		la.int_0014 = (int)(long)fa.modem;
		memset(out, 0x5a, sizeof out);
		count = 16;
		result = 0xeeee;
		probe_calls = 0;
		probe_modem_seen = 0;
		probe_in_seen = 0;
		probe_out_seen = 0;
		v27tx_process(&la, out, &count, &result);
		diff_eq_int("v27tx_process: probe called once (%ld)",
			    probe_calls, 1, tag);
		diff_eq_int("v27tx_process: modem forwarded (%ld)",
			    probe_modem_seen == (void *)fa.modem, 1, tag);
		diff_eq_int("v27tx_process: in is dp->buf (%ld)",
			    probe_in_seen == la.buf, 1, tag);
		diff_eq_int("v27tx_process: out is the caller's out (%ld)",
			    probe_out_seen == out, 1, tag);
		diff_eq_int("v27tx_process: result got the sample count (%ld)",
			    result, 0, tag);
		diff_eq_int("v27tx_process: count cleared (%ld)", count, 0,
			    tag);
	}
	tag++;

	/*
	 * The BLOB's own v17tx_process/v21tx_process/v27tx_process/
	 * v29tx_process, driven through the SAME probe (its address is
	 * planted in `prm`/`txdata` exactly as above), proving the blob's
	 * adapter passes the same four things to `V??TX_modem` that ours
	 * does -- this is the actual differential half of the check; the
	 * block above establishes what "correct" looks like against the
	 * object's own field layout.
	 */
	{
		struct v17tx_fixture fb;
		struct faxvmi_link lb;
		short out[16];
		unsigned short count, result;

		v17tx_fixture_build(&fb);
		put_i(fb.prm, V17TXP_INT_0008, 1);
		put_ptr(fb.prm, V17TXP_PROCESS, (void *)probe_tx_handler);
		poison_link(&lb, 0xb0);
		lb.buf = (unsigned short *)(void *)(fb.modem + 0);
		lb.int_0014 = (int)(long)fb.modem;
		memset(out, 0x5a, sizeof out);
		count = 16;
		result = 0xeeee;
		probe_calls = 0;
		probe_modem_seen = 0;
		probe_in_seen = 0;
		probe_out_seen = 0;
		ref_v17tx_process(&lb, out, &count, &result);
		diff_eq_int("ref_v17tx_process: probe called once (%ld)",
			    probe_calls, 1, tag);
		diff_eq_int("ref_v17tx_process: modem forwarded (%ld)",
			    probe_modem_seen == (void *)fb.modem, 1, tag);
		diff_eq_int("ref_v17tx_process: in is dp->buf (%ld)",
			    probe_in_seen == lb.buf, 1, tag);
		diff_eq_int("ref_v17tx_process: out is the caller's out (%ld)",
			    probe_out_seen == out, 1, tag);
	}
	tag++;

	{
		struct v21tx_fixture fb;
		struct faxvmi_link lb;
		short out[16];
		unsigned short count, result;

		v21tx_fixture_build(&fb);
		put_i(fb.prm, V21TXP_INT_0004, 1);
		put_ptr(fb.prm, V21TXP_PROCESS, (void *)probe_tx_handler);
		poison_link(&lb, 0xb0);
		lb.buf = (unsigned short *)(void *)(fb.modem + 0);
		lb.int_0014 = (int)(long)fb.modem;
		memset(out, 0x5a, sizeof out);
		count = 16;
		result = 0xeeee;
		probe_calls = 0;
		probe_modem_seen = 0;
		probe_in_seen = 0;
		probe_out_seen = 0;
		ref_v21tx_process(&lb, out, &count, &result);
		diff_eq_int("ref_v21tx_process: probe called once (%ld)",
			    probe_calls, 1, tag);
		diff_eq_int("ref_v21tx_process: modem forwarded (%ld)",
			    probe_modem_seen == (void *)fb.modem, 1, tag);
		diff_eq_int("ref_v21tx_process: in is dp->buf (%ld)",
			    probe_in_seen == lb.buf, 1, tag);
		diff_eq_int("ref_v21tx_process: out is the caller's out (%ld)",
			    probe_out_seen == out, 1, tag);
	}
	tag++;

	{
		struct v29tx_fixture fb;
		struct faxvmi_link lb;
		short out[16];
		unsigned short count, result;

		v29tx_fixture_build(&fb);
		put_i(fb.prm, V29TXP_INT_0008, 1);
		put_ptr(fb.prm, V29TXP_PROCESS, (void *)probe_tx_handler);
		poison_link(&lb, 0xb0);
		lb.buf = (unsigned short *)(void *)(fb.modem + 0);
		lb.int_0014 = (int)(long)fb.modem;
		memset(out, 0x5a, sizeof out);
		count = 16;
		result = 0xeeee;
		probe_calls = 0;
		probe_modem_seen = 0;
		probe_in_seen = 0;
		probe_out_seen = 0;
		ref_v29tx_process(&lb, out, &count, &result);
		diff_eq_int("ref_v29tx_process: probe called once (%ld)",
			    probe_calls, 1, tag);
		diff_eq_int("ref_v29tx_process: modem forwarded (%ld)",
			    probe_modem_seen == (void *)fb.modem, 1, tag);
		diff_eq_int("ref_v29tx_process: in is dp->buf (%ld)",
			    probe_in_seen == lb.buf, 1, tag);
		diff_eq_int("ref_v29tx_process: out is the caller's out (%ld)",
			    probe_out_seen == out, 1, tag);
	}
	tag++;

	{
		struct v27tx_fixture fb;
		struct faxvmi_link lb;
		short out[16];
		unsigned short count, result;

		v27tx_fixture_build(&fb);
		put_i(fb.txdata, V27TXP_INT_0008, 1);
		put_ptr(fb.txdata, V27TXP_PROCESS, (void *)probe_tx_handler);
		poison_link(&lb, 0xb0);
		lb.buf = (unsigned short *)(void *)(fb.modem + 0);
		lb.int_0014 = (int)(long)fb.modem;
		memset(out, 0x5a, sizeof out);
		count = 16;
		result = 0xeeee;
		probe_calls = 0;
		probe_modem_seen = 0;
		probe_in_seen = 0;
		probe_out_seen = 0;
		ref_v27tx_process(&lb, out, &count, &result);
		diff_eq_int("ref_v27tx_process: probe called once (%ld)",
			    probe_calls, 1, tag);
		diff_eq_int("ref_v27tx_process: modem forwarded (%ld)",
			    probe_modem_seen == (void *)fb.modem, 1, tag);
		diff_eq_int("ref_v27tx_process: in is dp->buf (%ld)",
			    probe_in_seen == lb.buf, 1, tag);
		diff_eq_int("ref_v27tx_process: out is the caller's out (%ld)",
			    probe_out_seen == out, 1, tag);
	}
	tag++;

	return diff_end();
}

/*
 * ---------------------------------------------------------------------
 * The five `*_control` adapters (finding F10010): a tail call that swaps
 * `dp` for `dp->int_0014` and forwards the caller's argument unchanged.
 * Each is driven off a REAL handle from the matching create adapter --
 * already proven correct above -- and its effect is read back through the
 * matching, already-tested status adapter: if `v??_control` forwarded the
 * wrong handle or the wrong argument, the callee either diverges (a
 * differing return code) or touches the wrong memory (a differing status
 * readback, or a crash `make one` reports as the whole binary failing).
 * D955/F8587: `dp` is poisoned and its canary checked unchanged after every
 * call, exactly as the delete/status adapters above.
 */

static int
run_control(void)
{
	long tag = 0;

	diff_begin("v17rx_control/v21tx_control/v21rx_control/v27tx_control/"
		   "v27rx_control");

	{
		struct v17rx_cfg c = V17RX_CFG;
		struct v17rx_ctl arg;
		struct faxvmi_link la, lb, la0, lb0;
		struct v17_status sa, sb;
		int reta, retb;

		poison_link(&la, 0xc0);
		poison_link(&lb, 0xc0);
		v17rx_create(&la, &c);
		ref_v17rx_create(&lb, &c);

		memset(&arg, 0, sizeof arg);
		arg.int_0004 = 12345;
		arg.flags_0c = V17RXCTL_CLEAR_STATE0;
		arg.flags_0d = 0;
		la0 = la;
		lb0 = lb;
		reta = v17rx_control(&la, &arg);
		retb = ref_v17rx_control(&lb, &arg);
		diff_eq_int("v17rx_control: return (%ld)", reta, retb, tag);
		diff_eq_int("v17rx_control: dp unchanged, ours (%ld)",
			    memcmp(&la, &la0, sizeof la), 0, tag);
		diff_eq_int("v17rx_control: dp unchanged, ref (%ld)",
			    memcmp(&lb, &lb0, sizeof lb), 0, tag);

		memset(&sa, 0x5a, sizeof sa);
		memset(&sb, 0x5a, sizeof sb);
		v17rx_status(&la, &sa);
		ref_v17rx_status(&lb, &sb);
		diff_eq_int("v17rx_control: status readback identical (%ld)",
			    memcmp(&sa, &sb, sizeof sa), 0, tag);

		V17RX_delete((void *)(long)la.int_0014);
		ref_V17RX_delete((void *)(long)lb.int_0014);
	}
	tag++;

	{
		struct v21tx_cfg c = V21TX_CFG;
		struct v21tx_ctl arg;
		struct faxvmi_link la, lb, la0, lb0;
		struct v21_status sa, sb;
		int reta, retb;

		poison_link(&la, 0xc1);
		poison_link(&lb, 0xc1);
		v21tx_create(&la, &c);
		ref_v21tx_create(&lb, &c);

		memset(&arg, 0, sizeof arg);
		arg.int_0004 = 555;
		arg.scale = 42;
		arg.mask = V21TXCTL_SET_TXFLAGS_BIT2;
		arg.flags = V21TXCTL_SET_PARAMS_INT0004;
		la0 = la;
		lb0 = lb;
		reta = v21tx_control(&la, &arg);
		retb = ref_v21tx_control(&lb, &arg);
		diff_eq_int("v21tx_control: return (%ld)", reta, retb, tag);
		diff_eq_int("v21tx_control: dp unchanged, ours (%ld)",
			    memcmp(&la, &la0, sizeof la), 0, tag);
		diff_eq_int("v21tx_control: dp unchanged, ref (%ld)",
			    memcmp(&lb, &lb0, sizeof lb), 0, tag);

		memset(&sa, 0x5a, sizeof sa);
		memset(&sb, 0x5a, sizeof sb);
		v21tx_status(&la, &sa);
		ref_v21tx_status(&lb, &sb);
		diff_eq_int("v21tx_control: status readback identical (%ld)",
			    memcmp(&sa, &sb, sizeof sa), 0, tag);

		V21TX_delete((void *)(long)la.int_0014);
		ref_V21TX_delete((void *)(long)lb.int_0014);
	}
	tag++;

	{
		struct v21rx_cfg c = V21RX_CFG;
		struct v21rx_ctl arg;
		struct faxvmi_link la, lb, la0, lb0;
		struct v21_status sa, sb;
		int reta, retb;

		poison_link(&la, 0xc2);
		poison_link(&lb, 0xc2);
		v21rx_create(&la, &c);
		ref_v21rx_create(&lb, &c);

		memset(&arg, 0, sizeof arg);
		arg.int_0004 = 987;
		arg.flags = V21RXCTL_SET_HDX_INT0000;
		la0 = la;
		lb0 = lb;
		reta = v21rx_control(&la, &arg);
		retb = ref_v21rx_control(&lb, &arg);
		diff_eq_int("v21rx_control: return (%ld)", reta, retb, tag);
		diff_eq_int("v21rx_control: dp unchanged, ours (%ld)",
			    memcmp(&la, &la0, sizeof la), 0, tag);
		diff_eq_int("v21rx_control: dp unchanged, ref (%ld)",
			    memcmp(&lb, &lb0, sizeof lb), 0, tag);

		memset(&sa, 0x5a, sizeof sa);
		memset(&sb, 0x5a, sizeof sb);
		v21rx_status(&la, &sa);
		ref_v21rx_status(&lb, &sb);
		diff_eq_int("v21rx_control: status readback identical (%ld)",
			    memcmp(&sa, &sb, sizeof sa), 0, tag);

		V21RX_delete((void *)(long)la.int_0014);
		ref_V21RX_delete((void *)(long)lb.int_0014);
	}
	tag++;

	{
		struct v27tx_cfg c = V27TX_CFG;
		struct faxvmi_link la, lb, la0, lb0;
		unsigned char sa[64], sb[64];
		unsigned char req[20];
		int reta, retb;

		poison_link(&la, 0xc3);
		poison_link(&lb, 0xc3);
		v27tx_create(&la, &c);
		ref_v27tx_create(&lb, &c);

		/* A non-null request, so this exercises V27TX_control's body
		 * (rate/prm/pps reads through `modem`) and not just its
		 * `req == 0` early return -- see V27TXCTL_* in v27fax.h. */
		memset(req, 0, sizeof req);
		put_i(req, V27TXCTL_INT_0004, 555);
		put_i(req, V27TXCTL_SCALE_MUL, 7);
		put_i(req, V27TXCTL_INT_0010, 3);
		req[V27TXCTL_MASK] = V27TXCTL_MASK_HANDLE_FLAG_04;
		req[V27TXCTL_FLAGS] = V27TXCTL_FLAGS_FORCE_INT_0008;

		la0 = la;
		lb0 = lb;
		reta = v27tx_control(&la, req);
		retb = ref_v27tx_control(&lb, req);
		diff_eq_int("v27tx_control: return (%ld)", reta, retb, tag);
		diff_eq_int("v27tx_control: dp unchanged, ours (%ld)",
			    memcmp(&la, &la0, sizeof la), 0, tag);
		diff_eq_int("v27tx_control: dp unchanged, ref (%ld)",
			    memcmp(&lb, &lb0, sizeof lb), 0, tag);

		memset(sa, 0x5a, sizeof sa);
		memset(sb, 0x5a, sizeof sb);
		v27tx_status(&la, sa);
		ref_v27tx_status(&lb, sb);
		diff_eq_int("v27tx_control: status readback identical (%ld)",
			    memcmp(sa, sb, sizeof sa), 0, tag);

		V27TX_delete((void *)(long)la.int_0014);
		ref_V27TX_delete((void *)(long)lb.int_0014);
	}
	tag++;

	{
		struct v27rx_cfg c = V27RX_CFG;
		struct faxvmi_link la, lb, la0, lb0;
		unsigned char sa[64], sb[64];
		int reta, retb;
		unsigned char req[16];

		poison_link(&la, 0xc4);
		poison_link(&lb, 0xc4);
		v27rx_create(&la, &c);
		ref_v27rx_create(&lb, &c);

		/* Same request shape `t_v27rxcontrol.c` exercises directly
		 * against `V27RX_control`: int_0004 at +0x04, mask at +0x0c
		 * (see that file's own `struct req`). */
		memset(req, 0, sizeof req);
		put_i(req, 0x04, 12345);
		req[0x0c] = 0x08;
		la0 = la;
		lb0 = lb;
		reta = v27rx_control(&la, req);
		retb = ref_v27rx_control(&lb, req);
		diff_eq_int("v27rx_control: return (%ld)", reta, retb, tag);
		diff_eq_int("v27rx_control: dp unchanged, ours (%ld)",
			    memcmp(&la, &la0, sizeof la), 0, tag);
		diff_eq_int("v27rx_control: dp unchanged, ref (%ld)",
			    memcmp(&lb, &lb0, sizeof lb), 0, tag);

		memset(sa, 0x5a, sizeof sa);
		memset(sb, 0x5a, sizeof sb);
		v27rx_status(&la, sa);
		ref_v27rx_status(&lb, sb);
		diff_eq_int("v27rx_control: status readback identical (%ld)",
			    memcmp(sa, sb, sizeof sa), 0, tag);

		V27RX_delete((void *)(long)la.int_0014);
		ref_V27RX_delete((void *)(long)lb.int_0014);
	}
	tag++;

	/*
	 * The last three, F10107: `v17tx_control`/`v29tx_control`/
	 * `v29rx_control`.  Unlike the fixtures above the TX handle here comes
	 * from a REAL `v??tx_create` -- unblocked in the same wave `run_tx_
	 * create_v17`/`v29` already proved correct, so this file's own
	 * "TX side has no create" intro comment no longer applies to these
	 * three; a real handle is simpler and stronger than a hand-built one.
	 */
	{
		struct v17tx_cfg c = V17TX_CFG;
		struct v17tx_control_req arg;
		struct faxvmi_link la, lb, la0, lb0;
		struct v17_status sa, sb;
		int reta, retb;

		poison_link(&la, 0xc5);
		poison_link(&lb, 0xc5);
		v17tx_create(&la, &c);
		ref_v17tx_create(&lb, &c);

		memset(&arg, 0, sizeof arg);
		arg.int_0004 = 4321;
		arg.scale_mul = 5;
		arg.int_0010 = 999;
		arg.ctl0 = V17TXCTL_CTL0_BIT2;
		arg.ctl1 = V17TXCTL_CTL1_BIT4;
		la0 = la;
		lb0 = lb;
		reta = v17tx_control(&la, &arg);
		retb = ref_v17tx_control(&lb, &arg);
		diff_eq_int("v17tx_control: return (%ld)", reta, retb, tag);
		diff_eq_int("v17tx_control: dp unchanged, ours (%ld)",
			    memcmp(&la, &la0, sizeof la), 0, tag);
		diff_eq_int("v17tx_control: dp unchanged, ref (%ld)",
			    memcmp(&lb, &lb0, sizeof lb), 0, tag);

		memset(&sa, 0x5a, sizeof sa);
		memset(&sb, 0x5a, sizeof sb);
		v17tx_status(&la, &sa);
		ref_v17tx_status(&lb, &sb);
		diff_eq_int("v17tx_control: status readback identical (%ld)",
			    memcmp(&sa, &sb, sizeof sa), 0, tag);

		V17TX_delete((void *)(long)la.int_0014);
		ref_V17TX_delete((void *)(long)lb.int_0014);
	}
	tag++;

	{
		struct v29tx_cfg c = V29TX_CFG;
		struct v29tx_control_req arg;
		struct faxvmi_link la, lb, la0, lb0;
		unsigned char sa[64], sb[64];
		int reta, retb;

		poison_link(&la, 0xc6);
		poison_link(&lb, 0xc6);
		v29tx_create(&la, &c);
		ref_v29tx_create(&lb, &c);

		memset(&arg, 0, sizeof arg);
		arg.int_0004 = 1234;
		arg.int_0008 = 3;
		arg.ctl0 = V29TXCTL_CTL0_BIT2;
		arg.ctl1 = V29TXCTL_CTL1_BIT4;
		la0 = la;
		lb0 = lb;
		reta = v29tx_control(&la, &arg);
		retb = ref_v29tx_control(&lb, &arg);
		diff_eq_int("v29tx_control: return (%ld)", reta, retb, tag);
		diff_eq_int("v29tx_control: dp unchanged, ours (%ld)",
			    memcmp(&la, &la0, sizeof la), 0, tag);
		diff_eq_int("v29tx_control: dp unchanged, ref (%ld)",
			    memcmp(&lb, &lb0, sizeof lb), 0, tag);

		memset(sa, 0x5a, sizeof sa);
		memset(sb, 0x5a, sizeof sb);
		v29tx_status(&la, sa);
		ref_v29tx_status(&lb, sb);
		diff_eq_int("v29tx_control: status readback identical (%ld)",
			    memcmp(sa, sb, sizeof sa), 0, tag);

		V29TX_delete((void *)(long)la.int_0014);
		ref_V29TX_delete((void *)(long)lb.int_0014);
	}
	tag++;

	{
		struct v29rx_cfg c = V29RX_CFG;
		struct v29rx_control_req arg;
		struct faxvmi_link la, lb, la0, lb0;
		unsigned char sa[64], sb[64];
		int reta, retb;

		poison_link(&la, 0xc7);
		poison_link(&lb, 0xc7);
		v29rx_create(&la, &c);
		ref_v29rx_create(&lb, &c);

		memset(&arg, 0, sizeof arg);
		arg.int_0004 = 6789;
		arg.ctl0 = V29RXCTL_CTL0_BIT3;
		arg.ctl1 = V29RXCTL_CTL1_BIT4;
		la0 = la;
		lb0 = lb;
		reta = v29rx_control(&la, &arg);
		retb = ref_v29rx_control(&lb, &arg);
		diff_eq_int("v29rx_control: return (%ld)", reta, retb, tag);
		diff_eq_int("v29rx_control: dp unchanged, ours (%ld)",
			    memcmp(&la, &la0, sizeof la), 0, tag);
		diff_eq_int("v29rx_control: dp unchanged, ref (%ld)",
			    memcmp(&lb, &lb0, sizeof lb), 0, tag);

		memset(sa, 0x5a, sizeof sa);
		memset(sb, 0x5a, sizeof sb);
		v29rx_status(&la, sa);
		ref_v29rx_status(&lb, sb);
		diff_eq_int("v29rx_control: status readback identical (%ld)",
			    memcmp(sa, sb, sizeof sa), 0, tag);

		V29RX_delete((void *)(long)la.int_0014);
		ref_V29RX_delete((void *)(long)lb.int_0014);
	}
	tag++;

	return diff_end();
}

/* ------------------------------------------------------------------- */

int
main(void)
{
	int bad = 0;

	bad |= run_rx_create();
	bad |= run_tx_create_v17();
	bad |= run_tx_create_v21();
	bad |= run_tx_create_v27();
	bad |= run_tx_create_v29();
	bad |= run_rx_delete();
	bad |= run_rx_status();
	bad |= run_rx_process();
	bad |= run_tx_delete();
	bad |= run_tx_status();
	bad |= run_tx_process();
	bad |= run_control();

	return bad;
}

/*
 * v8dp.h -- the V.8 datapump wrapper.
 *
 * The thin layer between slmodemd's datapump interface and the handshake
 * itself: it owns the `struct v8`, sets up the call menu from the modem's
 * parameters, and refuses any sample rate but 9600.
 */

#ifndef DSPLIB_V8DP_H
#define DSPLIB_V8DP_H

#include "dsplib/dp.h"
#include "dsplib/v8.h"
#include "dsplib/modem_params.h"

/* The id this datapump registers under. */
#define DP_V8	8

/*
 * The datapump ids V.8 can hand over to.  Two of them are also ids a call can
 * be STARTED with, meaning "this may end up as V.90 or V.92"; the other two
 * are only ever arrived at, by winning the modulation negotiation.  They are
 * the standard numbers, which is why 34 and 32 are what they are.
 */
#define DP_V32	32
#define DP_V34	34
#define DP_V90	90
#define DP_V92	92

/* The only rate the handshake runs at. */
#define V8_DP_RATE	9600

/*
 * Where the negotiated result is left for whoever asked for the call.
 *
 * `dspinfo` below is the SAME 16-byte host block `include/dsplib/
 * modem_params.h`'s `struct dsp_info` already names in full (finding F822,
 * derived from `vpcm_delete`/`dp_runtime_create`): both are reached the same
 * way, `modem_get_param(modem, MDMPRM_DSPINFO)`, and this wrapper only ever
 * touches the block's last two words -- `qc_lapm` at +0x08, `qc_index` at
 * +0x0c -- leaving `connection_type`/`clock_deviation` at +0x00/+0x04 to
 * whichever datapump actually negotiates a line rate.  No separate type is
 * declared for it; `struct v8_dp` below points at `struct dsp_info` directly
 * (finding F10197).
 */

/*
 * The wrapper's object, 52 bytes.  The first five words are the datapump
 * header every pump in this library shares; the rest is V.8's.
 */
struct v8_dp {
	int			id;		/* +0x00  always DP_V8 */
	void			*modem;		/* +0x04 */
	int			status;		/* +0x08  the shared datapump
						 * header's `status` (dp.h:38);
						 * kept `int`, the sibling there
						 * is `unsigned` */
	struct dp_operations	*op;		/* +0x0c */
	struct v8_dp		*self;		/* +0x10 */
	int			answerer;	/* +0x14 */
	int			want;		/* +0x18  the requested id */
	int			f1c;		/* +0x1c */
	int			idle_timer;	/* +0x20  the idle timer, counted
						 * down by `count` each call */
	/*
	 * MDMPRM_DSPINFO is an address, not a number: the wrapper writes the
	 * negotiated result into it when the handshake finishes.
	 */
	struct dsp_info		*dspinfo;	/* +0x24 */
	struct v8_cm		*cm;		/* +0x28 */
	int			last_status;	/* +0x2c  the previous status, kept
						 * to detect a change */
	struct v8			*v8;	/* +0x30 */
};

/*
 * `v8_op` is NOT declared here -- file-local in v8.c, on the same evidence as
 * b103.h's note.  Take it from `harness_reg_ours.ops[0]` after `dp_v8_init()`.
 *
 * `v8_create`, `v8_delete` and `v8_process` are file-local in the object too:
 * `nm` shows the object's FILE `v8.c` unit holding a lower-case `t` for all
 * three, with `v8_process` in the SAME unit as `v8_op`, so they are `static`
 * in v8.c and have no declarations here.  A test reaches `create` and
 * `destroy` through the table `dp_v8_init` registers, and `v8_process` out of
 * it as well, exactly as `v22.h` and `v23.h` record.
 */

/**
 * @brief Register the V.8 datapump.
 *
 * Called from prop_dp_init, which discards the result -- the object's
 * `prop_dp_init` calls this and then zeroes its own return value. The
 * `int` return type is the object's own: `dp_v8_init` ends with
 * `xor %eax,%eax`, which a `void` body cannot emit (finding F7860).
 *
 * @return Always 0.
 */
int dp_v8_init(void);

/** @brief Deregister the V.8 datapump. */
void dp_v8_exit(void);

#endif /* DSPLIB_V8DP_H */

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
	int			f08;		/* +0x08 */
	struct dp_operations	*op;		/* +0x0c */
	struct v8_dp		*self;		/* +0x10 */
	int			answerer;	/* +0x14 */
	int			want;		/* +0x18  the requested id */
	int			f1c;		/* +0x1c */
	int			f20;		/* +0x20 */
	/*
	 * MDMPRM_DSPINFO is an address, not a number: the wrapper writes the
	 * negotiated result into it when the handshake finishes.
	 */
	struct dsp_info		*dspinfo;	/* +0x24 */
	struct v8_cm		*cm;		/* +0x28 */
	int			f2c;		/* +0x2c */
	struct v8			*v8;	/* +0x30 */
};

/**
 * @brief Create the V.8 datapump wrapper.
 *
 * The three dp_operations entry points (this, v8_delete(), v8_process())
 * are all file-static in the object; they are declared here so a test can
 * call each side by name (finding F221's `ref_` aliases) rather than
 * pulling `create`/`destroy` out of what dp_v8_init() registers. Nothing
 * in `make phase` asserts that our linkage matches the original's, and
 * three of this tree's four datapumps already export what the blob keeps
 * local.
 *
 * @param modem     The owning modem object.
 * @param id        Requested datapump id (expected #DP_V8).
 * @param caller    Non-zero if this end originated the call.
 * @param srate     Sample rate; anything but 9600 is refused.
 * @param max_frag  Maximum fragment size.
 * @param op        Operations table to install into the wrapper.
 * @return The new `struct dp *`, or NULL on a refused sample rate.
 */
struct dp *v8_create(void *modem, int id, int caller, int srate, int max_frag,
		     struct dp_operations *op);

/** @brief Destroy a V.8 datapump wrapper created by v8_create(). */
int v8_delete(struct dp *dp);

/**
 * @brief One buffer through the handshake.
 *
 * Returns a DPSTAT_* code, and when the negotiation finishes it publishes
 * the result and asks the modem to change datapump.
 *
 * @param dp     The wrapper.
 * @param in     Input samples.
 * @param out    Output samples.
 * @param count  Sample count.
 * @return A DPSTAT_* status code.
 */
int v8_process(struct dp *dp, void *in, void *out, int count);

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

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

/*
 * The id this datapump registers under is `DP_V8`, and the ids it can hand
 * over to are `DP_V32`, `DP_V34`, `DP_V90` and `DP_V92`.  All five are
 * slmodemd's own enumerators now, from `enum DP_ID` in the vendored
 * `<modem_defs.h>` that `dsplib/dp.h` above pulls in; this header used to
 * `#define` all five, which would have rewritten each enumerator's own
 * definition into `8 = 8`.  Two of the four -- V.90 and V.92 -- are also ids
 * a call can be STARTED with, meaning "this may end up as V.90 or V.92"; the
 * other two are only ever arrived at, by winning the modulation negotiation.
 * They are the standard numbers, which is why 34 and 32 are what they are.
 * Findings F8402 and F8410.
 */

/* The only rate the handshake runs at. */
#define V8_DP_RATE	9600

/* Where the negotiated result is left for whoever asked for the call. */
struct v8_dspinfo {
	unsigned char	pad00[8];
	int		f08;			/* +0x08 */
	int		f0c;			/* +0x0c */
};

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
	struct v8_dspinfo	*dspinfo;	/* +0x24 */
	struct v8_cm		*cm;		/* +0x28 */
	int			f2c;		/* +0x2c */
	struct v8			*v8;	/* +0x30 */
};

/*
 * The three dp_operations entry points.
 *
 * All three are file-static in the object, and `v8_process` has always been
 * declared here; the other two are declared now for the same reason.  Since
 * finding F221 the object's copies carry `ref_` aliases, so a test can call
 * both sides by name rather than taking `create` and `destroy` out of what
 * `dp_v8_init` registers.  Nothing in `make phase` asserts that our linkage
 * matches the original's, and three of this tree's four datapumps already
 * export what the blob keeps local.
 */
struct dp *v8_create(struct modem *modem, enum DP_ID id, int caller, int srate, int max_frag,
		     struct dp_operations *op);
int v8_delete(struct dp *dp);

/*
 * One buffer through the handshake.  Returns a DPSTAT_* code, and when the
 * negotiation finishes it publishes the result and asks the modem to change
 * datapump.
 */
int v8_process(struct dp *dp, void *in, void *out, int count);

/*
 * Register the datapump.  Called from prop_dp_init, which discards the
 * result -- the object's `prop_dp_init` (+0x1c) calls this and then zeroes
 * %eax for its own return.  The `int` is the object's: `dp_v8_init` ends
 * `xor %eax,%eax` and a `void` body cannot emit that.  Finding F7860.
 */
int dp_v8_init(void);

/* And deregister it. */
void dp_v8_exit(void);

#endif /* DSPLIB_V8DP_H */

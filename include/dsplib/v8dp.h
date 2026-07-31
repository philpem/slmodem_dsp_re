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

/* The id this datapump registers under. */
#define DP_V8	8

/* The only rate the handshake runs at. */
#define V8_DP_RATE	9600

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
	int			dspinfo;	/* +0x24 */
	struct v8_cm		*cm;		/* +0x28 */
	int			f2c;		/* +0x2c */
	struct v8			*v8;	/* +0x30 */
};

/* Register the datapump.  Called from prop_dp_init. */
void dp_v8_init(void);

/* And deregister it. */
void dp_v8_exit(void);

#endif /* DSPLIB_V8DP_H */

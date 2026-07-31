/*
 * v8neg.h -- one end of a V.8 call, for the interop tier.
 *
 * Shared by the two drivers: t_spandsp_v8neg.c runs both ends in one process
 * and t_spandsp_v8sock.c runs them in two, with the audio crossing a socket.
 * The negotiation is the same code in both; only the transport differs, which
 * is the point of splitting it out.
 *
 * The audio crosses at 8000 Hz -- the rate a SIP leg carries -- and the
 * reconstructed rational resampler sits on our side of it, because the
 * handshake itself runs at 9600.
 */

#ifndef DSPLIB_INTEROP_V8NEG_H
#define DSPLIB_INTEROP_V8NEG_H

#include "spandsp.h"

#include "dsplib/v8.h"
#include "dsplib/fixedrc.h"

#define NATIVE_RATE	9600
#define SPANDSP_RATE	8000

/* 8000 -> 9600 and 9600 -> 8000, the modes call.c uses for a 9600 host. */
#define RC_UP		2
#define RC_DOWN		3

/* One 20 ms frame at 8000 Hz. */
#define V8NEG_FRAME	160

/* Room for the same frame at 9600, with slack for the resampler's jitter. */
#define V8NEG_NATIVE	256

/* Long enough for a negotiation, short enough to fail in finite time. */
#define V8NEG_MAX_FRAMES 600

/* What V8Process returns once the far end's message has been taken. */
#define V8NEG_STATUS_DONE 13

/* Our end of the call. */
struct side {
	struct v8	*v8;
	struct v8_cm	cm;
	struct rc	*up;		/* 8000 -> 9600, towards us   */
	struct rc	*down;		/* 9600 -> 8000, towards them */
	int		status;		/* what V8Process last said   */
	int		best;		/* and the highest it reached */
	int		negotiated;
};

/*
 * Build our end.  `mode` is 1 to answer and 0 to call -- the two shapes
 * v8handshakinit builds, and the reason V8Create takes a mode at all.
 */
int side_create(struct side *s, int mode);
void side_delete(struct side *s);

/*
 * One frame in and one frame out, both at 8000 Hz.  Returns how many samples
 * came out, which is not a constant: the resampler holds back whatever it
 * cannot yet turn into a whole output sample.
 */
int side_frame(struct side *s, const short *in, int n_in, short *out,
	       int out_max);

/* Fill in what SpanDSP's end should offer, to match ours. */
void v8neg_spandsp_parms(v8_parms_t *parms);

/* SpanDSP's status codes, for the log. */
const char *v8neg_status_name(int st);

/*
 * Print what our end decided, and return non-zero if it is what was offered.
 * Must be called after the negotiation: it runs V8UpdateModemParameters,
 * which is what the datapump layer does on status 13 and is where the
 * received menu becomes the intersection of the two.
 */
int side_report(struct side *s, const char *who);

#endif /* DSPLIB_INTEROP_V8NEG_H */

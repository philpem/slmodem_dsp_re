/*
 * v8neg.h -- one end of a V.8 call, for the interop tier.
 *
 * Deliberately free of any SpanDSP header, and reached through a table of
 * function pointers rather than by calling V8Process directly, because the
 * same code has to build two ways:
 *
 *   64-bit against the reconstruction   -- t_spandsp_v8neg, v8peer
 *   32-bit against the blob             -- v8peer_ref
 *
 * The blob is i386 and SpanDSP on this host is amd64, so those two can never
 * share an address space.  That is what the socket driver is for: put the
 * peer in its own process and SpanDSP can be made to talk to the original
 * object file, which is the only way to ask "would the original have done
 * this too?" of anything above the level of a single function call.
 *
 * The audio crosses at 8000 Hz -- the rate a SIP leg carries -- with the
 * rational resampler on our side of it, because the handshake runs at 9600.
 */

#ifndef DSPLIB_INTEROP_V8NEG_H
#define DSPLIB_INTEROP_V8NEG_H

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

/*
 * Which implementation a side runs.  Every entry is a public symbol of the
 * object, which is what makes the blob usable here at all: `objcopy
 * --redefine-syms` gives it a `ref_` prefix and both sets can be linked into
 * one program.
 */
struct v8_ops {
	struct v8 *(*create)(const struct v8_cfg *cfg);
	void	(*destroy)(struct v8 *v);
	int	(*process)(struct v8 *v, const short *in, short *out, int n);
	int	(*get_message)(struct v8 *v, unsigned char *out, int *count);
	int	(*update)(struct v8 *v, struct v8_cm *out);
	struct rc *(*rc_create)(int mode);
	void	(*rc_destroy)(struct rc *h);
	void	(*rc_resample)(struct rc *h, const short *in, int n_in,
			       short *out, int *n_out);
};

/* The reconstruction.  The blob's table is built where the blob is linked. */
extern const struct v8_ops v8neg_ours;

/* Our end of the call. */
struct side {
	const struct v8_ops *ops;
	struct v8	*v8;
	struct v8_cm	cm;
	struct rc	*up;		/* 8000 -> 9600, towards us   */
	struct rc	*down;		/* 9600 -> 8000, towards them */
	int		status;		/* what V8Process last said   */
	int		best;		/* and the highest it reached */
	int		negotiated;
};

/*
 * The menu this end offers, as the flag bytes `initTxSequence` reads.  Each
 * bit is one bit of one V.8 octet: b0 bit 5 is V.34, b0 bit 7 is V.32, b1
 * bit 4 is V.23, b1 bit 5 is V.21, and b1 bit 6 asks for the V-series call
 * function.
 */
#define V8NEG_B0_WIDE	0xa0		/* V.34 and V.32          */
#define V8NEG_B1_WIDE	0x70		/* V-series, V.21, V.23   */
#define V8NEG_B0_NARROW	0x80		/* V.32 only              */
#define V8NEG_B1_NARROW	0x60		/* V-series and V.21      */

/*
 * Build our end.  `mode` is 1 to answer and 0 to call -- the two shapes
 * v8handshakinit builds, and the reason V8Create takes a mode at all.
 */
int side_create(struct side *s, const struct v8_ops *ops, int mode,
		unsigned char b0, unsigned char b1);
void side_delete(struct side *s);

/*
 * One frame in and one frame out, both at 8000 Hz.  Returns how many samples
 * came out, which is not a constant: the resampler holds back whatever it
 * cannot yet turn into a whole output sample.
 */
int side_frame(struct side *s, const short *in, int n_in, short *out,
	       int out_max);

/*
 * What the agreed menu has to come to, once the two offers are intersected.
 *
 * Both `clear` fields matter as much as the `set` ones.  If the two ends
 * offer the same menu the intersection is a no-op, and a walk that never
 * ran -- wrong buffer, wrong mask, wrong continuation test -- would leave
 * exactly the bits this end planted and pass.  Only a call where the two
 * offers differ can tell those apart.
 */
struct v8neg_expect {
	unsigned char	b0_set;
	unsigned char	b0_clear;
	unsigned char	b1_set;
	unsigned char	b1_clear;
};

/* All four modulations, for a call where both ends offer the same. */
#define V8NEG_EXPECT_WIDE	{ 0xa0, 0x00, 0x70, 0x00 }

/*
 * V.32 and the V-series call function survive; V.34 and V.23 have to be
 * gone.  V.21 is not asserted either way -- the original can never withdraw
 * it, see findings.
 */
#define V8NEG_EXPECT_NARROW	{ 0x80, 0x20, 0x40, 0x10 }

/* Longest received message the interop tier keeps. */
#define V8NEG_MSG_MAX	32

/*
 * Everything a side decided, so that two of them can be compared rather than
 * merely printed side by side and looked at.
 */
struct side_result {
	int		ok;
	int		best;		/* the highest V8Process status */
	int		msg_len;
	unsigned char	msg[V8NEG_MSG_MAX];
	unsigned char	b0, b1, b2;	/* the agreed menu */
};

/*
 * Print what our end decided and check it against `e`.  Returns non-zero if
 * it holds, and fills `r` if it is not null.  Must be called after the
 * negotiation: it runs V8UpdateModemParameters, which is what the datapump
 * layer does on status 13 and is where the received menu becomes the
 * intersection of the two.
 */
int side_check(struct side *s, const char *who, const struct v8neg_expect *e,
	       struct side_result *r);

#endif /* DSPLIB_INTEROP_V8NEG_H */

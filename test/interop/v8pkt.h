/*
 * v8pkt.h -- one frame of audio on the socket between the two processes.
 *
 * A datagram each way, in strict alternation: the SpanDSP side sends first,
 * the peer answers exactly one frame per frame received, and the SpanDSP side
 * ends the call with a stop frame.  Nothing polls and nothing can deadlock.
 *
 * Kept header-only and free of both SpanDSP and dsplib so that the 32-bit
 * peer and the 64-bit driver agree on the layout without either of them
 * dragging in the other's headers.  The count is sent explicitly because the
 * resampler does not produce a constant number of samples per frame.
 */

#ifndef DSPLIB_INTEROP_V8PKT_H
#define DSPLIB_INTEROP_V8PKT_H

#include <stddef.h>
#include <sys/socket.h>
#include <sys/types.h>

/* Room for a 20 ms frame at 8000 Hz, doubled for the resampler's jitter. */
#define V8PKT_MAX	320

/* `n` of -1 means the call is over. */
#define V8PKT_STOP	(-1)

struct v8pkt {
	int	n;
	short	s[V8PKT_MAX];
};

/* Longest received message carried back.  Must match V8NEG_MSG_MAX. */
#define V8PKT_MSG_MAX	32

/*
 * What the peer decided, sent once after the stop frame.  Without this the
 * driver would see only an exit status, and two peers whose decoded messages
 * differed would still both exit 0 as long as the bits the expectation names
 * happened to match.  Every field is fixed width and explicitly padded, since
 * one end of this socket is i386 and the other amd64.
 */
struct v8pkt_result {
	int		ok;
	int		best;
	int		msg_len;
	unsigned char	b0, b1, b2, pad;
	unsigned char	msg[V8PKT_MSG_MAX];
};

static int
v8pkt_send(int fd, const struct v8pkt *p)
{
	size_t len = sizeof(p->n);

	if (p->n > 0)
		len += (size_t)p->n * sizeof(p->s[0]);
	return send(fd, p, len, 0) == (ssize_t)len ? 0 : -1;
}

static int
v8pkt_recv(int fd, struct v8pkt *p)
{
	ssize_t got = recv(fd, p, sizeof(*p), 0);

	if (got < (ssize_t)sizeof(p->n))
		return -1;
	if (p->n > V8PKT_MAX)
		return -1;
	return 0;
}

#endif /* DSPLIB_INTEROP_V8PKT_H */

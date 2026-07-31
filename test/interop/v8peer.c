/*
 * v8peer.c -- one end of a V.8 call, as its own process.
 *
 * Built twice.  Once 64-bit against the reconstruction, and once 32-bit with
 * -DV8PEER_REF against the blob, where every entry point is the original's
 * under its `ref_` name.  The two builds are the same program: the only
 * difference is which table of function pointers `side` is handed.
 *
 * That second build is the point of the whole exercise.  The blob is i386 and
 * SpanDSP on this host is amd64, so they cannot be linked into one program;
 * with the audio going over a socket they do not have to be.  It makes the
 * comparison the differential harness cannot: not "does the reconstruction
 * compute the same bytes as the original", but "does a third-party modem
 * negotiate the same call with each of them".
 *
 * Audio arrives and leaves on file descriptor 3, one frame per datagram, in
 * strict alternation.  The exit status is the verdict: 0 if the negotiation
 * completed and the agreed menu was what the caller said to expect.
 */

#include <errno.h>
#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <sys/socket.h>
#include <sys/time.h>
#include <unistd.h>

#include "v8neg.h"
#include "v8pkt.h"

/* The socket the driver hands us, already connected. */
#define PEER_FD		3

/* How long to wait for the far end before giving up on it. */
#define PEER_TIMEOUT	20

#ifdef V8PEER_REF

extern struct v8 *ref_V8Create(const struct v8_cfg *cfg);
extern void ref_V8Delete(struct v8 *v);
extern int ref_V8Process(struct v8 *v, const short *in, short *out, int n);
extern int ref_V8GetMessage(struct v8 *v, unsigned char *out, int *count);
extern int ref_V8UpdateModemParameters(struct v8 *v, struct v8_cm *out);
extern struct rc *ref_RcFixed_Create(int mode);
extern void ref_RcFixed_Delete(struct rc *h);
extern void ref_RcFixed_Resample(struct rc *h, const short *in, int n_in,
				 short *out, int *n_out);

static const struct v8_ops peer_ops = {
	.create		= ref_V8Create,
	.destroy	= ref_V8Delete,
	.process	= ref_V8Process,
	.get_message	= ref_V8GetMessage,
	.update		= ref_V8UpdateModemParameters,
	.rc_create	= ref_RcFixed_Create,
	.rc_destroy	= ref_RcFixed_Delete,
	.rc_resample	= ref_RcFixed_Resample,
};

#define PEER_NAME	"blob"

#else

#define peer_ops	v8neg_ours
#define PEER_NAME	"ours"

#endif

static void
usage(void)
{
	fprintf(stderr,
		"usage: v8peer <mode> <b0> <b1> "
		"<b0set> <b0clear> <b1set> <b1clear>\n"
		"  mode 1 answers the call, 0 places it\n");
	exit(2);
}

int
main(int argc, char **argv)
{
	struct side us;
	struct side_result r;
	struct v8pkt_result res;
	struct v8neg_expect expect;
	struct v8pkt in, out;
	struct timeval tv;
	int mode;
	unsigned char b0, b1;
	int ok;

	if (argc != 8)
		usage();

	mode = atoi(argv[1]);
	b0 = (unsigned char)strtoul(argv[2], NULL, 0);
	b1 = (unsigned char)strtoul(argv[3], NULL, 0);
	expect.b0_set = (unsigned char)strtoul(argv[4], NULL, 0);
	expect.b0_clear = (unsigned char)strtoul(argv[5], NULL, 0);
	expect.b1_set = (unsigned char)strtoul(argv[6], NULL, 0);
	expect.b1_clear = (unsigned char)strtoul(argv[7], NULL, 0);

	tv.tv_sec = PEER_TIMEOUT;
	tv.tv_usec = 0;
	setsockopt(PEER_FD, SOL_SOCKET, SO_RCVTIMEO, &tv, sizeof(tv));

	if (!side_create(&us, &peer_ops, mode, b0, b1)) {
		printf("    %s: could not build its end\n", PEER_NAME);
		fflush(stdout);
		return 2;
	}

	for (;;) {
		if (v8pkt_recv(PEER_FD, &in) != 0) {
			printf("    %s: lost the far end (%s)\n", PEER_NAME,
			       strerror(errno));
			fflush(stdout);
			return 3;
		}
		if (in.n == V8PKT_STOP)
			break;

		out.n = side_frame(&us, in.s, (int)in.n, out.s,
				   (int)(sizeof(out.s) / sizeof(out.s[0])));
		if (v8pkt_send(PEER_FD, &out) != 0) {
			printf("    %s: could not send (%s)\n", PEER_NAME,
			       strerror(errno));
			fflush(stdout);
			return 3;
		}
	}

	printf("    %s: V8Process reached status %d\n", PEER_NAME, us.best);
	ok = us.negotiated && side_check(&us, PEER_NAME, &expect, &r);
	fflush(stdout);

	/*
	 * And hand the whole verdict back, not just an exit status.  Two peers
	 * whose decoded messages differed would otherwise both exit 0 as long
	 * as the bits the expectation happens to name still matched.
	 */
	memset(&res, 0, sizeof(res));
	res.ok = ok;
	res.best = r.best;
	res.msg_len = r.msg_len;
	res.b0 = r.b0;
	res.b1 = r.b1;
	res.b2 = r.b2;
	memcpy(res.msg, r.msg, sizeof(res.msg));
	send(PEER_FD, &res, sizeof(res), 0);

	side_delete(&us);
	return ok ? 0 : 1;
}

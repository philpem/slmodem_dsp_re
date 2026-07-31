/*
 * t_spandsp_v8sock.c -- SpanDSP against a peer in another process.
 *
 * The negotiation itself is already proved in one process by
 * t_spandsp_v8neg.c, which is the readable version and where a failure is
 * easiest to diagnose.  This exists for something that cannot be done in one
 * process at all: running the SAME negotiation against the original object
 * file.
 *
 * The blob is i386 and the SpanDSP built here is amd64, so the two cannot be
 * linked together.  Put the peer behind a socket and the constraint goes
 * away.  Then every call below is run twice, once against the reconstruction
 * and once against the blob, and the two are compared -- which is a different
 * question from the one the differential harness asks.  That one asks whether
 * the reconstruction computes the same bytes; this one asks whether a modem
 * written by someone else negotiates the same call with each.
 *
 * The peer is `build/test/v8peer` and `build/test/v8peer_ref`; both take the
 * mode, the menu and the expected outcome on the command line and answer with
 * their exit status.
 */

#include <errno.h>
#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <sys/socket.h>
#include <sys/time.h>
#include <sys/wait.h>
#include <unistd.h>

#include "v8spandsp.h"
#include "v8neg.h"
#include "v8pkt.h"

/* Where the peer expects to find the socket. */
#define PEER_FD		3

static int checks;
static int failures;

static void
check(const char *what, int got, int want)
{
	checks++;
	if (got == want)
		return;
	failures++;
	printf("  FAIL %-52s got %d, want %d\n", what, got, want);
}

/* What one call came to, so the two peers can be compared. */
struct outcome {
	int			exit_status;
	int			sp_status;
	int			sp_call_function;
	uint32_t		sp_modulations;
	int			frames;
	int			have_result;
	struct v8pkt_result	peer;
};

static struct outcome now;

static void
result_handler(void *user_data, v8_parms_t *result)
{
	(void)user_data;
	now.sp_status = result->status;
	now.sp_call_function = result->jm_cm.call_function;
	now.sp_modulations = result->jm_cm.modulations;
}

/*
 * SpanDSP's end, and the one that decides when the call is over.  Sends
 * first, so the two processes stay in step without either having to poll.
 */
static int
run_spandsp(int fd, int calling, uint32_t menu)
{
	v8_state_t *them;
	v8_parms_t parms;
	struct v8pkt out, in;
	struct timeval tv;
	int frame;
	int got;

	tv.tv_sec = 20;
	tv.tv_usec = 0;
	setsockopt(fd, SOL_SOCKET, SO_RCVTIMEO, &tv, sizeof(tv));

	v8neg_spandsp_parms(&parms, menu);
	them = v8_init(NULL, calling, &parms, result_handler, NULL);
	if (them == NULL)
		return -1;

	for (frame = 0; frame < V8NEG_MAX_FRAMES; frame++) {
		memset(out.s, 0, sizeof(out.s));
		got = v8_tx(them, out.s, V8NEG_FRAME);
		out.n = got > 0 ? got : V8NEG_FRAME;
		if (v8pkt_send(fd, &out) != 0)
			break;

		if (v8pkt_recv(fd, &in) != 0)
			break;
		if (in.n > 0)
			v8_rx(them, in.s, (int)in.n);

		if (now.sp_status == V8_STATUS_V8_CALL)
			break;
	}

	out.n = V8PKT_STOP;
	v8pkt_send(fd, &out);

	/*
	 * The peer answers the stop frame with its verdict.  The receive
	 * timeout set above is what keeps a peer that died first from hanging
	 * the driver here.
	 */
	if (recv(fd, &now.peer, sizeof(now.peer), 0)
	    == (ssize_t)sizeof(now.peer))
		now.have_result = 1;

	v8_free(them);
	now.frames = frame;
	return 0;
}

/*
 * One call: fork, hand the child one end of a datagram socketpair on fd 3,
 * exec the peer, and run SpanDSP on the other end.
 */
static int
run_call(const char *peer, int our_mode, unsigned char our_b0,
	 unsigned char our_b1, uint32_t their_menu,
	 const struct v8neg_expect *e, struct outcome *out)
{
	char a_mode[8], a_b0[8], a_b1[8];
	char a_s0[8], a_c0[8], a_s1[8], a_c1[8];
	int sv[2];
	pid_t pid;
	int wstatus = 0;

	memset(&now, 0, sizeof(now));
	now.sp_status = -1;
	now.sp_call_function = -1;

	if (socketpair(AF_UNIX, SOCK_DGRAM, 0, sv) != 0) {
		printf("  FAIL socketpair: %s\n", strerror(errno));
		return -1;
	}

	snprintf(a_mode, sizeof(a_mode), "%d", our_mode);
	snprintf(a_b0, sizeof(a_b0), "0x%02x", our_b0);
	snprintf(a_b1, sizeof(a_b1), "0x%02x", our_b1);
	snprintf(a_s0, sizeof(a_s0), "0x%02x", e->b0_set);
	snprintf(a_c0, sizeof(a_c0), "0x%02x", e->b0_clear);
	snprintf(a_s1, sizeof(a_s1), "0x%02x", e->b1_set);
	snprintf(a_c1, sizeof(a_c1), "0x%02x", e->b1_clear);

	pid = fork();
	if (pid < 0) {
		printf("  FAIL fork: %s\n", strerror(errno));
		close(sv[0]);
		close(sv[1]);
		return -1;
	}

	if (pid == 0) {
		close(sv[0]);
		if (sv[1] != PEER_FD) {
			dup2(sv[1], PEER_FD);
			close(sv[1]);
		}
		execl(peer, peer, a_mode, a_b0, a_b1, a_s0, a_c0, a_s1, a_c1,
		      (char *)NULL);
		fprintf(stderr, "  FAIL exec %s: %s\n", peer, strerror(errno));
		_exit(127);
	}

	close(sv[1]);
	if (run_spandsp(sv[0], our_mode == 1, their_menu) != 0) {
		printf("  FAIL could not build SpanDSP's end\n");
		failures++;
	}
	close(sv[0]);

	if (waitpid(pid, &wstatus, 0) != pid) {
		printf("  FAIL waitpid: %s\n", strerror(errno));
		return -1;
	}

	now.exit_status = WIFEXITED(wstatus) ? WEXITSTATUS(wstatus) : -1;
	*out = now;
	return 0;
}

/*
 * The same call against both peers, then a comparison.  The reconstruction
 * has to pass on its own terms; the blob's run is what says the bar was set
 * where the original actually stands.
 */
static void
compare_call(const char *title, int our_mode, unsigned char our_b0,
	     unsigned char our_b1, uint32_t their_menu,
	     const struct v8neg_expect *e)
{
	struct outcome ours, blob;

	printf("\n%s\n", title);
	fflush(stdout);

	if (run_call("build/test/v8peer", our_mode, our_b0, our_b1,
		     their_menu, e, &ours) != 0) {
		failures++;
		return;
	}
	printf("    spandsp: status %d (%s), call function %d,"
	       " modulations 0x%x, %d frames\n",
	       ours.sp_status, v8neg_status_name(ours.sp_status),
	       ours.sp_call_function, (unsigned)ours.sp_modulations,
	       ours.frames);

	if (run_call("build/test/v8peer_ref", our_mode, our_b0, our_b1,
		     their_menu, e, &blob) != 0) {
		failures++;
		return;
	}
	printf("    spandsp: status %d (%s), call function %d,"
	       " modulations 0x%x, %d frames\n",
	       blob.sp_status, v8neg_status_name(blob.sp_status),
	       blob.sp_call_function, (unsigned)blob.sp_modulations,
	       blob.frames);

	check("the reconstruction negotiated with SpanDSP", ours.exit_status,
	      0);
	check("the blob negotiated with SpanDSP", blob.exit_status, 0);
	check("SpanDSP reached the same status with both", ours.sp_status,
	      blob.sp_status);
	check("and read the same call function from both",
	      (int)ours.sp_call_function, (int)blob.sp_call_function);
	check("and the same modulation list from both",
	      (int)ours.sp_modulations, (int)blob.sp_modulations);
	/*
	 * How long it took, to the frame.  The two run the same state machine
	 * over the same samples, so anything but an exact match means one of
	 * them took a different path through it -- which the byte-for-byte
	 * differential would have caught only if its sweep reached that path.
	 */
	check("and took the same number of frames", ours.frames, blob.frames);

	/*
	 * And the peers' own verdicts, which SpanDSP cannot see.  Without
	 * these the test would pass on two peers that decoded different
	 * messages, so long as the bits the expectation names still agreed --
	 * the printed blocks would differ and nothing would notice.
	 */
	check("both peers reported back", ours.have_result && blob.have_result,
	      1);
	if (!ours.have_result || !blob.have_result)
		return;
	check("both reached the same V8Process status", ours.peer.best,
	      blob.peer.best);
	/* Anti-vacuity: two empty messages would compare equal. */
	check("the message was not empty", ours.peer.msg_len > 0, 1);
	check("both received a message of the same length", ours.peer.msg_len,
	      blob.peer.msg_len);
	check("and the same message",
	      memcmp(ours.peer.msg, blob.peer.msg,
		     sizeof(ours.peer.msg)) == 0, 1);
	check("and agreed the same menu",
	      ours.peer.b0 == blob.peer.b0 && ours.peer.b1 == blob.peer.b1
	      && ours.peer.b2 == blob.peer.b2, 1);
}

int
main(void)
{
	static const struct v8neg_expect wide = V8NEG_EXPECT_WIDE;
	static const struct v8neg_expect narrow = V8NEG_EXPECT_NARROW;

	printf("SpanDSP interop: the reconstruction and the blob, "
	       "each in its own process\n");

	compare_call("SpanDSP calls, the peer answers", 1,
		     V8NEG_B0_WIDE, V8NEG_B1_WIDE, V8NEG_OURS, &wide);
	compare_call("The peer calls, SpanDSP answers", 0,
		     V8NEG_B0_WIDE, V8NEG_B1_WIDE, V8NEG_OURS, &wide);
	compare_call("SpanDSP calls with a narrower menu", 1,
		     V8NEG_B0_WIDE, V8NEG_B1_WIDE, V8NEG_NARROW, &narrow);
	compare_call("The peer calls with a narrower menu", 0,
		     V8NEG_B0_NARROW, V8NEG_B1_NARROW, V8NEG_OURS, &narrow);

	printf("\n%s: %d checks, %d failures\n",
	       failures ? "FAIL" : "PASS", checks, failures);
	return failures != 0;
}

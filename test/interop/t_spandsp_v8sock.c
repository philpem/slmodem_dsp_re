/*
 * t_spandsp_v8sock.c -- the same negotiation, between two processes.
 *
 * t_spandsp_v8neg.c has both ends in one address space, which is convenient
 * but lets a mistake hide: a shared global, a buffer written by one end and
 * read by the other, an ordering that only works because the two run in
 * lock-step inside one loop.  Here the reconstruction and SpanDSP are
 * separate processes and the only thing that passes between them is audio,
 * over a datagram socket, one frame at a time -- which is what a call over a
 * SIP leg actually looks like.
 *
 * Both directions are run: SpanDSP calls and the reconstruction answers, then
 * the reconstruction calls and SpanDSP answers.
 *
 * The two processes take strict turns, so there is nothing to deadlock on:
 * SpanDSP sends first, the reconstruction always answers exactly one frame
 * per frame received, and SpanDSP ends the call with a stop frame.  A receive
 * timeout on both sides keeps a crashed peer from hanging the test.
 */

#include <errno.h>
#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <sys/socket.h>
#include <sys/time.h>
#include <sys/wait.h>
#include <unistd.h>

#include "v8neg.h"

/* One frame on the wire.  `n` of -1 means "the call is over". */
struct pkt {
	int32_t	n;
	int16_t	s[V8NEG_FRAME * 2];
};

#define PKT_STOP	(-1)

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

static void
set_timeout(int fd, int seconds)
{
	struct timeval tv;

	tv.tv_sec = seconds;
	tv.tv_usec = 0;
	setsockopt(fd, SOL_SOCKET, SO_RCVTIMEO, &tv, sizeof(tv));
}

static int
pkt_send(int fd, const struct pkt *p)
{
	size_t len = sizeof(p->n);

	if (p->n > 0)
		len += (size_t)p->n * sizeof(p->s[0]);
	return send(fd, p, len, 0) == (ssize_t)len ? 0 : -1;
}

static int
pkt_recv(int fd, struct pkt *p)
{
	ssize_t got = recv(fd, p, sizeof(*p), 0);

	if (got < (ssize_t)sizeof(p->n))
		return -1;
	if (p->n > (int32_t)(sizeof(p->s) / sizeof(p->s[0])))
		return -1;
	return 0;
}

/* ------------------------------------------------------------------ child */

/*
 * The reconstruction's end.  Answers every frame with one frame, until the
 * far end says the call is over.  Exits 0 only if it both completed the
 * negotiation and read the menu it was sent.
 */
static void
run_ours(int fd, int mode, const char *who)
{
	struct side us;
	struct pkt in, out;
	int ok;

	set_timeout(fd, 20);

	if (!side_create(&us, mode)) {
		printf("  FAIL %s could not build its end\n", who);
		fflush(stdout);
		_exit(2);
	}

	for (;;) {
		if (pkt_recv(fd, &in) != 0) {
			printf("  FAIL %s lost the far end (%s)\n", who,
			       strerror(errno));
			fflush(stdout);
			_exit(3);
		}
		if (in.n == PKT_STOP)
			break;

		out.n = side_frame(&us, in.s, (int)in.n, out.s,
				   (int)(sizeof(out.s) / sizeof(out.s[0])));
		if (pkt_send(fd, &out) != 0) {
			printf("  FAIL %s could not send (%s)\n", who,
			       strerror(errno));
			fflush(stdout);
			_exit(3);
		}
	}

	ok = us.negotiated && side_report(&us, who);
	fflush(stdout);
	side_delete(&us);
	_exit(ok ? 0 : 1);
}

/* ----------------------------------------------------------------- parent */

static int sp_status;
static int sp_call_function;
static uint32_t sp_modulations;

static void
result_handler(void *user_data, v8_parms_t *result)
{
	(void)user_data;
	sp_status = result->status;
	sp_call_function = result->jm_cm.call_function;
	sp_modulations = result->jm_cm.modulations;
}

/*
 * SpanDSP's end, and the one that decides when the call is over.  Sends
 * first, so the two processes stay in step without either having to poll.
 */
static int
run_theirs(int fd, int calling, int *frames)
{
	v8_state_t *them;
	v8_parms_t parms;
	struct pkt out, in;
	int frame;
	int got;

	set_timeout(fd, 20);

	v8neg_spandsp_parms(&parms);
	sp_status = -1;
	sp_call_function = -1;
	sp_modulations = 0;

	them = v8_init(NULL, calling, &parms, result_handler, NULL);
	if (them == NULL)
		return -1;

	for (frame = 0; frame < V8NEG_MAX_FRAMES; frame++) {
		memset(out.s, 0, sizeof(out.s));
		got = v8_tx(them, out.s, V8NEG_FRAME);
		out.n = got > 0 ? got : V8NEG_FRAME;
		if (pkt_send(fd, &out) != 0)
			break;

		if (pkt_recv(fd, &in) != 0)
			break;
		if (in.n > 0)
			v8_rx(them, in.s, (int)in.n);

		if (sp_status == V8_STATUS_V8_CALL)
			break;
	}

	out.n = PKT_STOP;
	pkt_send(fd, &out);

	v8_free(them);
	*frames = frame;
	return 0;
}

/* ------------------------------------------------------------------ calls */

static void
run_call(const char *title, int our_mode)
{
	int sv[2];
	pid_t pid;
	int frames = 0;
	int wstatus = 0;

	printf("\n%s\n", title);
	fflush(stdout);

	if (socketpair(AF_UNIX, SOCK_DGRAM, 0, sv) != 0) {
		printf("  FAIL socketpair: %s\n", strerror(errno));
		failures++;
		return;
	}

	pid = fork();
	if (pid < 0) {
		printf("  FAIL fork: %s\n", strerror(errno));
		failures++;
		close(sv[0]);
		close(sv[1]);
		return;
	}

	if (pid == 0) {
		close(sv[0]);
		run_ours(sv[1], our_mode, "ours");
		/* not reached */
	}

	close(sv[1]);
	if (run_theirs(sv[0], our_mode == 1, &frames) != 0) {
		printf("  FAIL could not build SpanDSP's end\n");
		failures++;
	}
	close(sv[0]);

	if (waitpid(pid, &wstatus, 0) != pid) {
		printf("  FAIL waitpid: %s\n", strerror(errno));
		failures++;
		return;
	}

	printf("  after %d frames (%.1f s of audio):\n", frames,
	       frames * (double)V8NEG_FRAME / SPANDSP_RATE);
	printf("    spandsp: status %d (%s), call function %d,"
	       " modulations 0x%x\n",
	       sp_status, v8neg_status_name(sp_status), sp_call_function,
	       (unsigned)sp_modulations);

	check("SpanDSP completed the negotiation", sp_status,
	      V8_STATUS_V8_CALL);
	check("SpanDSP read our call function", sp_call_function,
	      V8_CALL_V_SERIES);
	check("SpanDSP read our whole modulation list",
	      (int)(sp_modulations & (V8_MOD_V21 | V8_MOD_V23 | V8_MOD_V32
				      | V8_MOD_V34)),
	      V8_MOD_V21 | V8_MOD_V23 | V8_MOD_V32 | V8_MOD_V34);
	check("the other process exited normally", WIFEXITED(wstatus), 1);
	check("and it negotiated and read the menu",
	      WIFEXITED(wstatus) ? WEXITSTATUS(wstatus) : -1, 0);
}

int
main(void)
{
	printf("SpanDSP interop: a whole V.8 negotiation, over a socket\n");

	run_call("SpanDSP calls, the reconstruction answers", 1);
	run_call("The reconstruction calls, SpanDSP answers", 0);

	printf("\n%s: %d checks, %d failures\n",
	       failures ? "FAIL" : "PASS", checks, failures);
	return failures != 0;
}

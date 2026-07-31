/*
 * t_spandsp_v8neg.c -- a whole V.8 negotiation against an independent one.
 *
 * The signal-layer interop (t_spandsp_v8.c) proves the tones are the tones.
 * This proves the conversation: the reconstruction and SpanDSP each take one
 * end of a call and have to agree on a modulation.
 *
 * Both directions are run.  SpanDSP calls and the reconstruction answers;
 * then the reconstruction calls and SpanDSP answers.
 *
 * Both ends are in this process, which makes the failure modes easy to read.
 * t_spandsp_v8sock.c runs the same negotiation between two processes with the
 * audio crossing a socket, which is the arrangement a real call would have.
 */

#include <stdio.h>
#include <stdlib.h>
#include <string.h>

#include "v8neg.h"

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
 * Run one call to completion or to the frame limit.
 *
 * `our_mode` is 1 when the reconstruction answers and 0 when it calls;
 * SpanDSP takes the other end.
 */
static void
run_call(const char *title, int our_mode, int verbose)
{
	struct side us;
	v8_state_t *them;
	v8_parms_t parms;
	short to_us[V8NEG_FRAME * 2], to_them[V8NEG_FRAME * 2];
	int frame;
	int last_tx = -1, last_rx = -1, last_sub = -1, last_sp = -2;
	int n_to_us, n_to_them;

	printf("\n%s\n", title);

	if (!side_create(&us, our_mode)) {
		printf("  FAIL could not build our end\n");
		failures++;
		return;
	}

	v8neg_spandsp_parms(&parms);
	sp_status = -1;
	sp_call_function = -1;
	sp_modulations = 0;

	/* SpanDSP calls when we answer, and answers when we call. */
	them = v8_init(NULL, our_mode == 1, &parms, result_handler, NULL);
	if (them == NULL) {
		printf("  FAIL could not build SpanDSP's end\n");
		failures++;
		side_delete(&us);
		return;
	}

	memset(to_us, 0, sizeof(to_us));
	n_to_us = V8NEG_FRAME;

	for (frame = 0; frame < V8NEG_MAX_FRAMES; frame++) {
		int got;

		n_to_them = side_frame(&us, to_us, n_to_us, to_them,
				       (int)(sizeof(to_them)
					     / sizeof(to_them[0])));
		if (n_to_them > 0)
			v8_rx(them, to_them, n_to_them);

		memset(to_us, 0, sizeof(to_us));
		got = v8_tx(them, to_us, V8NEG_FRAME);
		n_to_us = got > 0 ? got : V8NEG_FRAME;

		if (verbose
		    && (us.v8->f9d4 != last_tx || us.v8->f9d6 != last_rx
			|| us.v8->f9d8 != last_sub || sp_status != last_sp)) {
			printf("  %4d  ours tx=%-3d rx=0x%-3x sub=0x%-3x"
			       " status=%-3d   spandsp=%d (%s)\n",
			       frame, us.v8->f9d4, us.v8->f9d6, us.v8->f9d8,
			       us.status, sp_status,
			       v8neg_status_name(sp_status));
			last_tx = us.v8->f9d4;
			last_rx = us.v8->f9d6;
			last_sub = us.v8->f9d8;
			last_sp = sp_status;
		}

		if (us.negotiated && sp_status == V8_STATUS_V8_CALL)
			break;
	}

	printf("  after %d frames (%.1f s of audio):\n", frame,
	       frame * (double)V8NEG_FRAME / SPANDSP_RATE);
	printf("    spandsp: status %d (%s), call function %d,"
	       " modulations 0x%x\n",
	       sp_status, v8neg_status_name(sp_status), sp_call_function,
	       (unsigned)sp_modulations);

	check("the reconstruction completed the negotiation", us.negotiated,
	      1);
	check("SpanDSP completed the negotiation", sp_status,
	      V8_STATUS_V8_CALL);
	check("SpanDSP read our call function", sp_call_function,
	      V8_CALL_V_SERIES);
	check("SpanDSP read our whole modulation list",
	      (int)(sp_modulations & (V8_MOD_V21 | V8_MOD_V23 | V8_MOD_V32
				      | V8_MOD_V34)),
	      V8_MOD_V21 | V8_MOD_V23 | V8_MOD_V32 | V8_MOD_V34);
	check("the reconstruction read SpanDSP's menu",
	      side_report(&us, "ours"), 1);

	v8_free(them);
	side_delete(&us);
}

int
main(int argc, char **argv)
{
	int verbose = argc > 1 && strcmp(argv[1], "-v") == 0;

	printf("SpanDSP interop: a whole V.8 negotiation, in one process\n");

	run_call("SpanDSP calls, the reconstruction answers", 1, verbose);
	run_call("The reconstruction calls, SpanDSP answers", 0, verbose);

	printf("\n%s: %d checks, %d failures\n",
	       failures ? "FAIL" : "PASS", checks, failures);
	return failures != 0;
}

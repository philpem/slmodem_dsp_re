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

#include "v8spandsp.h"
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
run_call(const char *title, int our_mode, unsigned char our_b0,
	 unsigned char our_b1, uint32_t their_menu,
	 const struct v8neg_expect *expect, uint32_t expect_mods, int verbose)
{
	struct side us;
	v8_state_t *them;
	v8_parms_t parms;
	short to_us[V8NEG_FRAME * 2], to_them[V8NEG_FRAME * 2];
	int frame;
	int last_tx = -1, last_rx = -1, last_sub = -1, last_sp = -2;
	int n_to_us, n_to_them;

	printf("\n%s\n", title);

	if (!side_create(&us, &v8neg_ours, our_mode, our_b0, our_b1)) {
		printf("  FAIL could not build our end\n");
		failures++;
		return;
	}

	v8neg_spandsp_parms(&parms, their_menu);
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
	/*
	 * Exactly, not masked: a spurious bit our encoder sets -- V.22, V.90
	 * -- is as much a misread menu as a missing one, and masking down to
	 * the four we care about would hide it.
	 */
	check("SpanDSP read our modulation list exactly",
	      (int)sp_modulations, (int)expect_mods);
	check("the reconstruction read SpanDSP's menu",
	      side_check(&us, "ours", expect), 1);

	v8_free(them);
	side_delete(&us);
}

int
main(int argc, char **argv)
{
	static const struct v8neg_expect wide = V8NEG_EXPECT_WIDE;
	static const struct v8neg_expect narrow = V8NEG_EXPECT_NARROW;
	int verbose = argc > 1 && strcmp(argv[1], "-v") == 0;

	printf("SpanDSP interop: a whole V.8 negotiation, in one process\n");

	/*
	 * Both ends offering the same four proves the negotiation runs, but
	 * not that either end computed an intersection: with nothing to
	 * remove, a menu walk that never ran looks the same as one that did.
	 */
	run_call("SpanDSP calls, the reconstruction answers", 1,
		 V8NEG_B0_WIDE, V8NEG_B1_WIDE, V8NEG_OURS,
		 &wide, V8NEG_OURS, verbose);
	run_call("The reconstruction calls, SpanDSP answers", 0,
		 V8NEG_B0_WIDE, V8NEG_B1_WIDE, V8NEG_OURS,
		 &wide, V8NEG_OURS, verbose);

	/*
	 * So run two more where the offers differ, one testing each half of
	 * the mapping.
	 *
	 * SpanDSP calls with less than we offer: V.34 and V.23 have to
	 * disappear from what our end agrees, which exercises the decoder,
	 * and the JM we build back carries the narrowed list, so SpanDSP's own
	 * report narrows too -- the same intersection seen from the other side
	 * of the wire.
	 */
	run_call("SpanDSP calls with a narrower menu", 1,
		 V8NEG_B0_WIDE, V8NEG_B1_WIDE, V8NEG_NARROW,
		 &narrow, V8NEG_NARROW, verbose);

	/*
	 * And we call with less than SpanDSP offers, which exercises the
	 * encoder: SpanDSP has to see exactly the two we asked for.  Narrowing
	 * SpanDSP's own parameters would not do it -- SpanDSP builds its JM
	 * from the CM it received, not from what it was configured with, so an
	 * answering SpanDSP echoes our list back whatever it was told to
	 * offer.
	 */
	run_call("The reconstruction calls with a narrower menu", 0,
		 V8NEG_B0_NARROW, V8NEG_B1_NARROW, V8NEG_OURS,
		 &narrow, V8NEG_NARROW, verbose);

	printf("\n%s: %d checks, %d failures\n",
	       failures ? "FAIL" : "PASS", checks, failures);
	return failures != 0;
}

/*
 * t_calldirect.c -- call_create, call_run, call_delete and call_GetSRegister
 * called BY NAME, with the whole state object compared after every block.
 *
 * All four are file statics in the object, and t_call opens by saying there is
 * "no way to call them by name" -- so it calls `dp_call_init` and takes the
 * three operations out of the registration log.  Finding F221 made that untrue:
 * the file-local symbols are globalized before they are renamed, so
 * `ref_call_run` and the rest link.  `call_GetSRegister` is not in the
 * operations table at all and had no route in whatsoever; it is tested here
 * for the first time.
 *
 * WHAT IS COMPARED, AND WHAT t_call COMPARES
 *
 * t_call checks the returned status, 200 output samples and eighteen fields.
 * `struct call_dp` is 1,480 bytes: two 320-byte scratch buffers, two ring
 * queues with their own data arrays, and a 368-byte `struct callprog` holding
 * the whole supervisor.  Eighteen fields do not cover it, and the two rings
 * are compared only as `memcmp(...) == 0`, which says a difference exists
 * without saying where.  Here the object is the comparison and a divergence
 * names its own field.
 *
 * The pulse dialler's view is compared too.  `MDMPRM_DP_ADDR` hands out a
 * `struct call` -- the same memory seen through pulse.c's eyes -- and nothing
 * has ever looked at what the dialler wrote into it.
 *
 * THE TWO SIDES CANNOT BE INTERLEAVED
 *
 * `CALLPROG_Dial` reaches the pulse dialler through the single
 * `MDMPRM_DP_ADDR` slot, so a block of one side followed by a block of the
 * other would have each of them dialling into the other's object.  t_call
 * drives each side to completion for that reason and so does this file, which
 * is why the per-block state is snapshotted rather than compared as it is
 * produced.
 *
 * THE MOTION GUARD SCANS, IT DOES NOT COMPARE THE ENDS
 *
 * Two objects that never moved compare equal, so each run asserts that the
 * state went somewhere.  Comparing the first block against the last does not
 * establish that: with a dial string too long to prefix the supervisor moves,
 * finishes and comes back to exactly where it started, and the endpoint form
 * of this guard reported that run as vacuous when it was not.  Every block is
 * compared against the first.
 *
 * POINTERS
 *
 * `op` must be the table create was passed and `self` the object itself; both
 * are checked and then zeroed.  `rc_in` and `rc_out` exist only when the host
 * rate is not 8 kHz, and the four inside `struct callprog` -- `dial`, `busy`,
 * `band`, `dtmf` -- only when that detector was configured, so all six become
 * a 0/1: whether the two sides built the same set is the whole of what can be
 * compared about an address.  `modem`, in both structures, is the value the
 * caller supplied and stays.
 */

#include <stdio.h>
#include <stdint.h>
#include <string.h>

#include "harness.h"
#include "dsplib/call.h"
#include "dsplib/dp.h"
#include "dsplib/callprog.h"
#include "dsplib/modem_params.h"
#include "dsplib/pulse.h"

/* By name.  File-static in the object; see finding F221. */
extern struct dp *ref_call_create(void *modem, int id, int caller, int srate,
				  int max_frag, struct dp_operations *op);
extern int ref_call_delete(struct dp *dp);
extern int ref_call_run(struct dp *dp, void *in, void *out, int count);
extern long ref_call_GetSRegister(void *modem, unsigned short num);

/* Only so `op` can be checked against the table that side was passed. */
extern void ref_dp_call_init(void);

#define MAXCALLS	90
#define MAXSAMP		200

static struct dp_operations *ops_ours, *ops_ref;

static struct dp_operations *
ops_of(int ref)
{
	struct reg_log *log = ref ? &harness_reg_ref : &harness_reg_ours;
	int i;

	harness_reg_reset();
	if (ref)
		ref_dp_call_init();
	else
		dp_call_init();
	for (i = 0; i < log->count; i++)
		if (log->id[i] == DP_CALL)
			return log->ops[i];
	return 0;
}

static int sreg56;
static long total_nonzero;
static int status_seen[16];

/* 550 Hz at amplitude 5000, the level and frequency the detectors respond to. */
static const short cycle[64] = {
	     0,    490,    975,   1451,   1913,   2357,   2778,   3172,
	  3536,   3865,   4157,   4410,   4619,   4785,   4904,   4976,
	  5000,   4976,   4904,   4785,   4619,   4410,   4157,   3865,
	  3536,   3172,   2778,   2357,   1913,   1451,    975,    490,
	     0,   -490,   -975,  -1451,  -1913,  -2357,  -2778,  -3172,
	 -3536,  -3865,  -4157,  -4410,  -4619,  -4785,  -4904,  -4976,
	 -5000,  -4976,  -4904,  -4785,  -4619,  -4410,  -4157,  -3865,
	 -3536,  -3172,  -2778,  -2357,  -1913,  -1451,   -975,   -490
};

#define TONE_STEP	9011		/* 550 * 64 * 2048 / 8000 */

#define SIG_SILENCE	0
#define SIG_TONE	1
#define SIG_NOISE	2

static short input[MAXCALLS][MAXSAMP];

static void
fill_input(int signal, int samples)
{
	unsigned long r = 7717;
	long n = 0;
	int b, i;

	for (b = 0; b < MAXCALLS; b++) {
		for (i = 0; i < samples; i++, n++) {
			switch (signal) {
			case SIG_TONE:
				input[b][i] = cycle[(int)(((n * TONE_STEP)
							  >> 11) & 63)];
				break;
			case SIG_NOISE:
				r = r * 1103515245u + 12345u;
				input[b][i] = (short)(((r >> 16) & 0x1fff)
						      - 0x1000);
				break;
			default:
				input[b][i] = 0;
				break;
			}
		}
	}
}

static void
params(const char *dialstr)
{
	harness_param_reset();
	harness_sreg_reset();

	harness_param_set(MDMPRM_DIALSTR, (long)(intptr_t)dialstr);
	harness_param_set(GetPulseDialDigitPattern, 1);
	harness_param_set(GetPulseDialingFlag, 1);
	harness_param_set(GetDialModifierValidation, 1);
	harness_param_set(GetDialPauseTime, 2);
	harness_param_set(GetComaPauseDurationLimit, 10);
	harness_param_set(GetPulseDialMakeTime, 33);
	harness_param_set(GetPulseDialBreakTime, 67);
	harness_param_set(GetPulseBetweenDigitsInterval, 8);
	harness_param_set(GetDTMFDialSpeed, 70);
	harness_param_set(GetDTMFHighToneLevel, 9);
	harness_param_set(GetDTMFHighAndLowToneLevelDifference, 2);
	harness_param_set(GetCallingToneFlag, 1);
	harness_param_set(GetHookFlashTime, 50);

	harness_param_set(GetCallProgressSamplesBufferLength, 160);
	harness_param_set(GetMinBusyCadenceOnTime, 4);
	harness_param_set(GetMaxBusyCadenceOnTime, 12);
	harness_param_set(GetMinBusyCadenceOffTime, 4);
	harness_param_set(GetMaxBusyCadenceOffTime, 12);
	harness_param_set(GetBusyDetectionCyclesNumber, 3);
	harness_param_set(GetBusyToneCallProgressFilterIndex, 0);
	harness_param_set(GetBusyToneDiffTime, 3);
	harness_param_set(GetDialToneCallProgressFilterIndex, 0);
	harness_param_set(GetDialToneDetectionThreshold, 40);
	harness_param_set(GetDialToneValidationTime, 10);
	harness_param_set(GetDialToneWaitTime, 100);

	harness_sreg_set(16, 1);	/* tone dialling when no prefix */
	harness_sreg_set(56, sreg56);
}

struct side {
	struct dp		*dp;
	struct call		call;		/* what MDMPRM_DP_ADDR hands out */
	short			out[MAXCALLS][MAXSAMP];
	int			rc[MAXCALLS];
	struct call_dp		snap[MAXCALLS];
	struct call		csnap[MAXCALLS];
	const void		*getsreg;	/* this side's own callback */
	int			calls;
};

static struct side side_a, side_b;
static const void *our_getsreg;

/*
 * The S-register adaptor is file-static now, so the test cannot name it.  It
 * is still reachable the way the supervisor holds it: build one call-setup
 * object through the registered table, take the callback create planted in
 * its configuration, and tear the object down again.
 */
static const void *
capture_getsreg(void)
{
	struct call dummy;
	struct dp *dp;
	const void *p;

	memset(&dummy, 0, sizeof(dummy));
	dummy.self = &dummy;
	dummy.modem = (void *)0xD1A1u;
	params("5551234");
	harness_param_set(MDMPRM_DP_ADDR, (long)(intptr_t)&dummy);
	dp = ops_ours->create((void *)0xD1A1u, DP_CALL, 1, 8000, 160, ops_ours);
	if (dp == 0)
		return 0;
	p = ((struct call_dp *)dp)->callprog.get_sreg;
	ops_ours->destroy(dp);
	return p;
}

static void
normalise_dp(struct call_dp *dst, const struct call_dp *src,
	     const struct dp_operations *op, const void *getsreg,
	     const char *side, long tag)
{
	char b[128];

	*dst = *src;

	snprintf(b, sizeof(b), "%s: op is the table create was passed (%%ld)",
		 side);
	diff_eq_int(b, src->op == op, 1, tag);
	snprintf(b, sizeof(b), "%s: self points at the object (%%ld)", side);
	diff_eq_int(b, src->self == src, 1, tag);
	/*
	 * The one function pointer in the object, and the reason this file
	 * can test `call_GetSRegister` at all: `call_create` plants it in the
	 * supervisor's configuration and nothing else ever names it.  Each
	 * side must have planted ITS OWN -- which is a stronger statement than
	 * "both are non-null", and the two addresses can never be equal.
	 */
	snprintf(b, sizeof(b), "%s: get_sreg is this side's own (%%ld)", side);
	diff_eq_int(b, (const void *)src->callprog.get_sreg == getsreg, 1,
		    tag);
	dst->callprog.get_sreg = 0;

	dst->op = 0;
	dst->self = 0;
	dst->rc_in = (struct rc *)(size_t)(src->rc_in != 0);
	dst->rc_out = (struct rc *)(size_t)(src->rc_out != 0);
	dst->callprog.dial = (struct cadence *)(size_t)
			     (src->callprog.dial != 0);
	dst->callprog.busy = (struct cadence *)(size_t)
			     (src->callprog.busy != 0);
	dst->callprog.band = (struct iir_filter *)(size_t)
			     (src->callprog.band != 0);
	dst->callprog.dtmf = (struct dual_tone *)(size_t)
			     (src->callprog.dtmf != 0);
}

static void
normalise_call(struct call *dst, const struct call *src, const char *side,
	       long tag)
{
	char b[128];

	*dst = *src;

	snprintf(b, sizeof(b), "%s: dialler view self-pointer (%%ld)", side);
	diff_eq_int(b, src->self == src, 1, tag);

	dst->self = 0;
	dst->dp_runtime = (void *)(size_t)(src->dp_runtime != 0);
}

/*
 * One side, driven to completion.  The store's single MDMPRM_DP_ADDR slot is
 * pointed at this side's own object first, so the two runs cannot reach into
 * each other.
 */
static void
drive(struct side *s, int ref, const char *dialstr, int srate, int samples,
      int calls)
{
	struct dp_operations *op = ref ? ops_ref : ops_ours;
	int n;

	memset(s->out, 0, sizeof(s->out));
	memset(s->rc, 0, sizeof(s->rc));
	memset(s->snap, 0, sizeof(s->snap));
	memset(s->csnap, 0, sizeof(s->csnap));
	memset(&s->call, 0, sizeof(s->call));
	s->call.self = &s->call;
	s->call.modem = (void *)0xD1A1u;
	s->calls = 0;
	s->dp = 0;

	params(dialstr);
	harness_param_set(MDMPRM_DP_ADDR, (long)(intptr_t)&s->call);

	s->dp = ref
		? ref_call_create((void *)0xD1A1u, DP_CALL, 1, srate, 160, op)
		: op->create((void *)0xD1A1u, DP_CALL, 1, srate, 160, op);
	if (s->dp == 0)
		return;

	s->getsreg = ref ? (const void *)ref_call_GetSRegister
			 : ((const struct call_dp *)s->dp)->callprog.get_sreg;

	for (n = 0; n < calls && n < MAXCALLS; n++) {
		s->rc[n] = ref
			   ? ref_call_run(s->dp, input[n], s->out[n], samples)
			   : op->process(s->dp, input[n], s->out[n], samples);
		normalise_dp(&s->snap[n], (struct call_dp *)s->dp, op,
			     s->getsreg, ref ? "ref" : "ours", n);
		normalise_call(&s->csnap[n], &s->call, ref ? "ref" : "ours", n);
		s->calls = n + 1;
	}
}

static int
run(const char *label, const char *dialstr, int signal, int srate, int samples,
    int calls)
{
	int n, i;

	diff_begin(label);
	fill_input(signal, samples);

	drive(&side_a, 1, dialstr, srate, samples, calls);
	drive(&side_b, 0, dialstr, srate, samples, calls);

	diff_eq_int("both created (%ld)", side_b.dp != 0 && side_a.dp != 0, 1,
		    0);
	diff_eq_int("same number of blocks (%ld)", side_b.calls, side_a.calls,
		    0);

	for (n = 0; n < side_a.calls && n < side_b.calls; n++) {
		diff_eq_int("block %ld: status", side_b.rc[n], side_a.rc[n], n);
		for (i = 0; i < samples; i++) {
			diff_eq_int("block %ld: sample", side_b.out[n][i],
				    side_a.out[n][i], n);
			if (side_a.out[n][i] != 0)
				total_nonzero++;
		}
		diff_eq_obj("datapump object", struct call_dp, &side_b.snap[n],
			    &side_a.snap[n], n);
		diff_eq_obj("dialler view", struct call, &side_b.csnap[n],
			    &side_a.csnap[n], n);
		if (side_a.rc[n] >= 0 && side_a.rc[n] < 16)
			status_seen[side_a.rc[n]]++;
	}

	/*
	 * Anti-vacuity, per run: the object has to have moved off what the
	 * first block left, or two datapumps idling in step would satisfy
	 * every comparison above.
	 */
	{
		int moved = 0;

		for (n = 1; n < side_a.calls; n++)
			if (memcmp(&side_a.snap[0], &side_a.snap[n],
				   sizeof(side_a.snap[0])) != 0)
				moved = 1;
		diff_eq_int("the state moved during the run (%ld)", moved, 1, 0);
	}

	if (side_a.dp != 0 && side_b.dp != 0)
		diff_eq_int("call_delete returns (%ld)",
			    ops_ours->destroy(side_b.dp),
			    ref_call_delete(side_a.dp), 0);
	side_a.dp = side_b.dp = 0;

	return diff_end();
}

int
main(void)
{
	int rc = 0;
	int i, distinct;

	diff_begin("call direct: the aliases link");
	ops_ref = ops_of(1);
	ops_ours = ops_of(0);
	diff_eq_int("both registered (%ld)", ops_ref != 0 && ops_ours != 0, 1,
		    0);
	diff_eq_int("ref_call_create resolves (%ld)",
		    (void *)ref_call_create != 0, 1, 0);
	diff_eq_int("ref_call_run resolves (%ld)", (void *)ref_call_run != 0,
		    1, 0);
	diff_eq_int("ref_call_delete resolves (%ld)",
		    (void *)ref_call_delete != 0, 1, 0);
	diff_eq_int("ref_call_GetSRegister resolves (%ld)",
		    (void *)ref_call_GetSRegister != 0, 1, 0);
	/*
	 * Our side's four entry points are file-static now, so what a caller
	 * can name is the table `dp_call_init` registered.  `create`,
	 * `destroy` and `process` must all be present; `call_GetSRegister`
	 * is reached below through the callback `create` plants.
	 */
	diff_eq_int("ours: create/destroy/process registered (%ld)",
		    ops_ours->create != 0 && ops_ours->destroy != 0
		    && ops_ours->process != 0, 1, 0);
	diff_eq_int("ref: process is ref_call_run (%ld)",
		    (void *)ops_ref->process == (void *)ref_call_run, 1, 0);
	if (ops_ref == 0 || ops_ours == 0)
		return diff_end();
	rc |= diff_end();

	/*
	 * call_GetSRegister is not in the operations table; the supervisor
	 * holds it only as the callback `call_create` planted, so that is
	 * how the test reaches our side too.  Every register, and the two
	 * ends of the range.
	 */
	our_getsreg = capture_getsreg();
	diff_begin("call_GetSRegister");
	{
		long (*our_get)(void *, unsigned short) =
			(long (*)(void *, unsigned short))our_getsreg;
		unsigned n;

		diff_eq_int("the callback was planted (%ld)",
			    our_getsreg != 0, 1, 0);
		if (our_getsreg == 0) {
			rc |= diff_end();
			return rc;
		}
		harness_sreg_reset();
		for (n = 0; n < 256; n++)
			harness_sreg_set(n, (long)(n * 7919u) & 0xffff);
		for (n = 0; n < 300; n++)
			diff_eq_int("S%ld",
				    our_get((void *)0xD1A1u,
					    (unsigned short)n),
				    ref_call_GetSRegister((void *)0xD1A1u,
							  (unsigned short)n),
				    (long)n);
		diff_eq_int("and it is not returning a constant (%ld)",
			    our_get((void *)0xD1A1u, 7)
			    != our_get((void *)0xD1A1u, 8), 1, 0);
	}
	rc |= diff_end();

	/*
	 * The fragment sizes that matter: one that divides the 48-sample
	 * block, one that does not, one smaller than a block, one larger than
	 * the ring.
	 */
	rc |= run("call: 160 samples, silence", "5551234", SIG_SILENCE, 8000,
		  160, 60);
	rc |= run("call: 48 samples", "5551234", SIG_SILENCE, 8000, 48, 90);
	rc |= run("call: 47 samples", "5551234", SIG_SILENCE, 8000, 47, 90);
	rc |= run("call: 1 sample", "5551234", SIG_SILENCE, 8000, 1, 90);
	rc |= run("call: 200 samples", "5551234", SIG_SILENCE, 8000, 200, 40);

	/*
	 * Both halves of the S56 split.  With 0, 1 or 3 no state listens for
	 * dial tone and the machine blind-dials on a timeout; with 2 or 4 it
	 * starts in DIALING and dial tone is listened for.  Only the second
	 * half ever puts samples on the line.
	 */
	for (i = 0; i <= 4; i++) {
		char lb[64];

		sreg56 = i;
		sprintf(lb, "call: S56=%d, tone", i);
		rc |= run(lb, "5551234", SIG_TONE, 8000, 160, 80);
	}
	sreg56 = 2;
	rc |= run("call: dial tone, long run", "5551234", SIG_TONE, 8000, 160,
		  90);
	sreg56 = 0;
	rc |= run("call: blind dial outlasts its timeout", "5551234",
		  SIG_SILENCE, 8000, 160, 90);
	rc |= run("call: noise", "5551234", SIG_NOISE, 8000, 160, 60);

	/* The two rates that get resamplers, and one that does not. */
	rc |= run("call: 9600, resampled", "5551234", SIG_TONE, 9600, 160, 60);
	rc |= run("call: 48000, resampled", "5551234", SIG_TONE, 48000, 160,
		  60);
	rc |= run("call: 11025, no converter", "5551234", SIG_TONE, 11025, 160,
		  40);

	/* Dial strings: prefixed, unprefixed, modifiers, and one too long. */
	rc |= run("call: explicit tone prefix", "T5551234", SIG_SILENCE, 8000,
		  160, 60);
	rc |= run("call: explicit pulse prefix", "P5551234", SIG_SILENCE, 8000,
		  160, 90);
	rc |= run("call: modifiers", "5W55@5", SIG_SILENCE, 8000, 160, 60);
	rc |= run("call: too long to prefix",
		  "5555555555555555555555555555555555555555555555555555555555"
		  "5555555", SIG_SILENCE, 8000, 160, 40);
	rc |= run("call: empty dial string", "", SIG_SILENCE, 8000, 160, 20);

	diff_begin("guards");
	{
		distinct = 0;
		for (i = 0; i < 16; i++)
			if (status_seen[i] != 0)
				distinct++;
		diff_eq_int("more than one status was returned (%ld)",
			    distinct >= 2, 1, distinct);
		diff_eq_int("DPSTAT_OK was returned (%ld)",
			    status_seen[DPSTAT_OK] > 0, 1,
			    status_seen[DPSTAT_OK]);
		diff_eq_int("the dialler reached the line (%ld samples)",
			    total_nonzero > 1000, 1, total_nonzero);
	}
	rc |= diff_end();

	return rc;
}

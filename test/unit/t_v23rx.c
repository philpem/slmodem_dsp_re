/*
 * t_v23rx.c -- differential test of the V.23 1200 bps receiver.
 *
 * Like the backward channel, this object does nothing at all until it has
 * heard carrier: FPM_TONE_detect has to report 1300 Hz twice before a single
 * bit comes out.  Feed it noise and both receivers sit in acquisition and
 * agree perfectly about it, which proves nothing.  So the stimulus comes from
 * v23FP_tx configured for the forward channel -- the same thing the far end
 * of a real call transmits, and already proved against the blob by t_v23tx --
 * with a leading run of mark bits standing in for the idle carrier.
 *
 * WHAT IS COMPARED, after every block:
 *
 *   - the return code
 *   - `*nbits` and the whole of the caller's bit array, both poisoned before
 *     each call.  That matters more here than it looks: v23FP_rx_progress
 *     writes NEITHER of them while acquiring or after giving up, so poisoning
 *     is the only way to tell "left alone" from "written with the right
 *     value", and the two sides have to agree about which it was.
 *   - the sample buffer, which is worked on in place
 *   - the object byte for byte apart from the fourteen pointer fields
 *   - and, through those pointers, all five heap buffers: the IIR state, the
 *     resampler history, and the demodulator's trace, FIR and IIR histories.
 *
 * The last of those is the point.  Bits alone would let a receiver that
 * arrived at the right answer by a different route -- a slicer holding a
 * different previous bit, a resampler half a phase out -- pass for a while and
 * then diverge somewhere with no test at all.
 */

#include <stdio.h>
#include <string.h>

#include "harness.h"
#include "dsplib/v23fp.h"

extern void *ref_v23FP_rx_create(void *rx, const void *cfg);
extern void ref_v23FP_rx_delete(void *rx);
extern short ref_v23FP_rx_progress(void *rx, short *samples, int count,
				   int *bits, int *nbits);

#define FW_MARK		1300
#define FW_SPACE	2100

static const short fw_period[3] = { 7, 7, 6 };

/* Coverage. */
static int saw_waiting;		/* returned 1, still acquiring          */
static int saw_frozen;		/* the state-10 block, gain frozen once */
static int saw_running;		/* returned 0                           */
static int saw_bits;		/* at least one bit came out            */
static int saw_one, saw_zero;	/* and both values did                  */
static int saw_discarded;	/* returned 0 but reported no bits      */
static int saw_lost;		/* gave up on carrier loss              */
static int saw_never;		/* gave up waiting for carrier          */

/*
 * The pointer fields.  Two kinds, and both have to be skipped for the same
 * reason -- the two sides hold different addresses -- but for different
 * underlying causes:
 *
 *   - coefficient pointers (agc alpha/beta, mrf coeff, fsd fir/iir,
 *     iir_coeff) point into .rodata, ours at our tables and the reference at
 *     the blob's.  t_v23filt already compares the global tables word for word;
 *     the AGC smoother coefficients are file-static in the original and so
 *     unreachable by name, but only element 0 is ever selected and it is
 *     16384 in both files.
 *   - buffer pointers (mrf.history, fsd.trace/fir_hist/iir_hist, iir_state,
 *     tone) are two heap allocations.  Their CONTENTS are compared below,
 *     which is the part that matters.
 */
static int
is_pointer_field(unsigned off)
{
	static const unsigned ptr[] = {
		__builtin_offsetof(struct v23rx, tone),
		__builtin_offsetof(struct v23rx, agc)
			+ __builtin_offsetof(struct fpm_agc_cfg, alpha),
		__builtin_offsetof(struct v23rx, agc)
			+ __builtin_offsetof(struct fpm_agc_cfg, beta),
		__builtin_offsetof(struct v23rx, det_agc)
			+ __builtin_offsetof(struct fpm_agc_cfg, alpha),
		__builtin_offsetof(struct v23rx, det_agc)
			+ __builtin_offsetof(struct fpm_agc_cfg, beta),
		__builtin_offsetof(struct v23rx, mrf)
			+ __builtin_offsetof(struct fpm_mrf_cfg, coeff),
		__builtin_offsetof(struct v23rx, mrf)
			+ __builtin_offsetof(struct fpm_mrf, history),
		__builtin_offsetof(struct v23rx, fsd)
			+ __builtin_offsetof(struct fpm_fsd_cfg, fir),
		__builtin_offsetof(struct v23rx, fsd)
			+ __builtin_offsetof(struct fpm_fsd_cfg, iir),
		__builtin_offsetof(struct v23rx, fsd)
			+ __builtin_offsetof(struct fpm_fsd, trace),
		__builtin_offsetof(struct v23rx, fsd)
			+ __builtin_offsetof(struct fpm_fsd, fir_hist),
		__builtin_offsetof(struct v23rx, fsd)
			+ __builtin_offsetof(struct fpm_fsd, iir_hist),
		__builtin_offsetof(struct v23rx, iir_coeff),
		__builtin_offsetof(struct v23rx, iir_state)
	};
	unsigned i;

	for (i = 0; i < sizeof(ptr) / sizeof(ptr[0]); i++)
		if (off >= ptr[i] && off < ptr[i] + sizeof(void *))
			return 1;
	return 0;
}

static void
compare_buf(const char *what, const short *a, const short *b, int n)
{
	int i;

	for (i = 0; i < n; i++)
		diff_eq_int(what, a[i], b[i], i);
}

static void
compare_obj(const char *what, const struct v23rx *ours, const void *refp)
{
	const unsigned char *a = (const unsigned char *)ours;
	const unsigned char *b = (const unsigned char *)refp;
	const struct v23rx *ref = (const struct v23rx *)refp;
	unsigned i;

	for (i = 0; i < sizeof(struct v23rx); i++) {
		if (is_pointer_field(i))
			continue;
		diff_eq_int(what, a[i], b[i], i);
	}

	/* And everything the skipped pointers point at. */
	compare_buf("iir state[%ld]", ours->iir_state, ref->iir_state,
		    4 * 4);
	compare_buf("mrf history[%ld]", ours->mrf.history, ref->mrf.history,
		    ours->mrf.history_len);
	compare_buf("fsd trace[%ld]", ours->fsd.trace, ref->fsd.trace,
		    ours->fsd.cfg.trace_len);
	compare_buf("fsd fir hist[%ld]", ours->fsd.fir_hist, ref->fsd.fir_hist,
		    ours->fsd.cfg.fir_taps);
	compare_buf("fsd iir hist[%ld]", ours->fsd.iir_hist, ref->fsd.iir_hist,
		    2 * ours->fsd.cfg.iir_len);
}

/*
 * Generate `n` samples of forward-channel FSK from `bits`, using the
 * transmitter this tree already proves against the blob.
 */
static void
generate(short *out, int n, const int *bits, int nbits)
{
	struct v23tx *tx;
	int at = 0;
	int done = 0;

	tx = v23FP_tx_create(NULL, FW_MARK, FW_SPACE, 3, fw_period, 0);
	while (done < n) {
		int chunk = n - done > 160 ? 160 : n - done;
		int used = 0;

		v23FP_tx_progress(tx, out + done, chunk, bits + at, &used);
		done += chunk;
		at += used;
		if (at + 32 > nbits)
			at = 0;
	}
	v23FP_tx_delete(tx);
}

/*
 * Run one signal through both receivers, `block` samples at a time, and
 * return how many bits came out of ours.
 *
 * `block` must not exceed 160: `det_buf` is that long and neither receiver
 * bounds the copy against it, so a larger block would corrupt both objects
 * identically and turn the byte comparison into a comparison of two equally
 * wrecked structures.
 */
static int
run(const char *what, const short *signal, int n, int block, int silence_limit,
    int stop_on_giveup)
{
	struct v23_cfg cfg;
	struct v23rx *ours;
	void *ref;
	short buf_a[256], buf_b[256];
	int bits_a[64], bits_b[64];
	int total = 0;
	int at;

	memset(&cfg, 0, sizeof(cfg));
	cfg.silence_limit = silence_limit;

	ours = v23FP_rx_create(NULL, &cfg);
	ref = ref_v23FP_rx_create(NULL, &cfg);
	compare_obj("after create, byte %ld", ours, ref);

	for (at = 0; at + block <= n; at += block) {
		short rc_a, rc_b;
		int was_frozen;
		int na, nb;
		int i;

		/* Each side needs its own copy: the block is worked on in place. */
		memcpy(buf_a, signal + at, block * sizeof(short));
		memcpy(buf_b, signal + at, block * sizeof(short));
		buf_a[block] = 0x5a5a;
		buf_b[block] = 0x5a5a;
		memset(bits_a, 0x5a, sizeof(bits_a));
		memset(bits_b, 0x5a, sizeof(bits_b));
		na = nb = 0x5a5a5a5a;

		was_frozen = ours->agc.freeze;
		rc_a = v23FP_rx_progress(ours, buf_a, block, bits_a, &na);
		rc_b = ref_v23FP_rx_progress(ref, buf_b, block, bits_b, &nb);

		diff_eq_int("return code", rc_a, rc_b, at / block);
		/*
		 * Compared unconditionally, including where neither side is
		 * supposed to have written: both must have left the poison.
		 */
		diff_eq_int("bits produced", na, nb, at / block);
		for (i = 0; i < 64; i++)
			diff_eq_int("bit[%ld]", bits_a[i], bits_b[i], i);
		for (i = 0; i < block; i++)
			diff_eq_int(what, buf_a[i], buf_b[i], i);
		diff_eq_int("wrote past the block", buf_a[block], 0x5a5a, 0);
		compare_obj("object after a block, byte %ld", ours, ref);

		if (rc_a == 0) {
			saw_running++;
			if (na == 0)
				saw_discarded++;
			for (i = 0; i < na && i < 64; i++) {
				if (bits_a[i])
					saw_one++;
				else
					saw_zero++;
			}
			if (na > 0) {
				saw_bits++;
				total += na;
			}
		} else if (rc_a == 1) {
			saw_waiting++;
		}

		/*
		 * State 10 is never visible from out here: the block that
		 * reaches it freezes the gain and moves to 11 before
		 * returning.  So the freeze itself is what gets counted, via
		 * the flag it sets.
		 */
		if (!was_frozen && ours->agc.freeze)
			saw_frozen++;
		if (rc_a == 2) {
			if (ours->rx_state > 9)
				saw_lost++;
			else
				saw_never++;
			if (stop_on_giveup)
				break;
		}
	}

	v23FP_rx_delete(ours);
	ref_v23FP_rx_delete(ref);
	return total;
}

#define NSAMP 24000		/* three seconds at 8 kHz */

/*
 * The acquisition deadline is 3000 calls of any length, so the silent buffer
 * has to be long enough for that many blocks with room to spare rather than
 * long enough in seconds.
 */
#define NQUIET (8 * 3200)

static short signal[NSAMP];
static short quiet[NQUIET];
static short acq_then_quiet[NSAMP];

int
main(void)
{
	static int idle[64], data[64];
	int rc = 0;
	int bits;
	int i;

	for (i = 0; i < 64; i++) {
		idle[i] = 1;			/* continuous mark */
		data[i] = (i / 2) & 1;		/* runs of both     */
	}

	/*
	 * Carrier first, then data.  FPM_TONE_detect works on a running
	 * average and the gate wants two reports of 1300 Hz, so the idle run
	 * has to be substantial -- a few hundred samples would leave both
	 * receivers waiting and the test would prove nothing.
	 */
	generate(signal, NSAMP / 4, idle, 64);
	generate(signal + NSAMP / 4, NSAMP - NSAMP / 4, data, 64);
	memset(quiet, 0, sizeof(quiet));

	/* The same acquisition, then a dead line. */
	memcpy(acq_then_quiet, signal, sizeof(acq_then_quiet));
	memset(acq_then_quiet + NSAMP / 2, 0,
	       (NSAMP - NSAMP / 2) * sizeof(short));

	diff_begin("v23FP_rx: acquiring carrier and demodulating");
	bits = run("filtered samples[%ld]", signal, NSAMP, 160, 100000, 1);
	printf("  160-sample blocks: %d bits out\n", bits);
	rc |= diff_end();

	diff_begin("v23FP_rx: other block sizes");
	/*
	 * 3:4 resampling means a block that is not a multiple of 4 leaves the
	 * resampler owing part of an output, and 5 samples per bit means a
	 * block that is not a multiple of 20 input samples ends part way
	 * through a bit.  80 is both; 60 is neither; 100 is one and not the
	 * other.  All are at or under det_buf's 160.
	 */
	run("block 60[%ld]", signal, NSAMP, 60, 100000, 1);
	run("block 80[%ld]", signal, NSAMP, 80, 100000, 1);
	run("block 100[%ld]", signal, NSAMP, 100, 100000, 1);
	rc |= diff_end();

	diff_begin("v23FP_rx: carrier lost after acquisition");
	/*
	 * A short silence limit, so the receiver gives up a few blocks into
	 * the dead half.  The blocks in between exercise the path that
	 * demodulates and then throws the bits away.
	 */
	run("lost carrier[%ld]", acq_then_quiet, NSAMP, 160, 200, 1);
	rc |= diff_end();

	diff_begin("v23FP_rx: carrier that never arrives");
	/*
	 * 20 ms is charged per CALL, not per sample, so the 60-second
	 * acquisition deadline is 3000 calls of any length.  8 samples keeps
	 * it cheap; the point is the counter and the give-up, not the DSP.
	 */
	run("never[%ld]", quiet, NQUIET, 8, 100000, 1);
	rc |= diff_end();

	diff_begin("v23FP_rx: coverage");
	diff_eq_int("it waited for carrier", saw_waiting > 0, 1, 0);
	diff_eq_int("the gain was frozen", saw_frozen > 0, 1, 0);
	diff_eq_int("carrier came up", saw_running > 0, 1, 0);
	diff_eq_int("bits were demodulated", saw_bits > 0, 1, 0);
	diff_eq_int("a one was received", saw_one > 0, 1, 0);
	diff_eq_int("a zero was received", saw_zero > 0, 1, 0);
	diff_eq_int("bits were demodulated and dropped", saw_discarded > 0, 1,
		    0);
	diff_eq_int("it gave up on a lost carrier", saw_lost > 0, 1, 0);
	diff_eq_int("it gave up waiting for one", saw_never > 0, 1, 0);
	/*
	 * The freeze happens on the one block that sees state 10, so exactly
	 * once per receiver that acquires -- five of the six runs above; the
	 * silent one never gets there.  Asserted rather than merely counted:
	 * a second freeze would mean the gate had been re-entered, which is
	 * the kind of thing that shows up only as a slow drift in the gain.
	 */
	diff_eq_int("the gain froze once per acquiring receiver", saw_frozen,
		    5, 0);
	printf("  waiting %d, running %d, froze %d, dropped %d, "
	       "ones %d, zeros %d\n",
	       saw_waiting, saw_running, saw_frozen, saw_discarded, saw_one,
	       saw_zero);
	rc |= diff_end();

	return rc;
}

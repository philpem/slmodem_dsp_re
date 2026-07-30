/*
 * t_b103fp.c -- differential test of the Bell 103 transmit path.
 *
 * ModDataB103 is the first function in this tree that produces a real signal:
 * bits in, 8 kHz Bell 103 FSK out.  That makes it worth more than its 99
 * bytes suggest -- it is also the stimulus generator for the receive-side
 * tests to come, so an error here would propagate into everything downstream
 * looking like a receiver bug.
 *
 * Objects are built with the *reference* B103FP_create, one per side, because
 * both sides mutate modulator and resampler state through the pointer at
 * +0x54 and sharing one object would have each side comparing against its own
 * writes.
 *
 * The three things worth proving, in order of how easily they hide:
 *
 *   1. the sample count.  FPM_FSM writes 24 samples per bit at 7200 Hz and
 *      FPM_MRF lifts that to 8000, so bits in and samples out are related by
 *      24 * 10/9 with a remainder that the resampler carries between calls.
 *      A test using a single fixed bit count would never see the remainder.
 *   2. that TxNoCarrierB103 advances the modulator by exactly as much as
 *      ModDataB103 would have.  That is its whole reason to exist -- muting
 *      the buffer instead would be indistinguishable over one call and wrong
 *      over two.
 *   3. that the output is actually a signal.  A reconstruction that emitted
 *      silence would agree with a blob that emitted silence.
 */

#include <math.h>
#include <string.h>

#include "harness.h"
#include "dsplib/b103fp.h"

extern void *ref_B103FP_create(void *state, const void *cfg);
extern void ref_B103FP_delete(void *state);
extern short ref_B103_CFG[];
extern short ref_ModDataB103(void *fp, const unsigned short *bits, short *out,
			     unsigned short nbits);
extern short ref_TxNoCarrierB103(void *fp, const unsigned short *bits,
				 short *out, unsigned short nbits);
extern int ref_CarrierDetectB103(void *fp);
extern short ref_DemodDataB103(void *fp, short *in, unsigned short *bits,
			       unsigned short count);
extern void *ref_FPM_TONE_create(void *state, void *cfg);
extern short ref_FPM_TONE_CFG[];

/* Coverage counters. */
static int saw_nonzero_output;
static int saw_silent_output;
static int distinct_counts[256];

/*
 * Compare the parts of the DSP block the transmit path touches.  The whole
 * 256-byte block cannot be memcmp'd: it holds four heap pointers that differ
 * between the two objects.
 */
static void
compare_tx_state(const char *what, struct b103_dsp *ours,
		 struct b103_dsp *ref, int tag)
{
	char buf[96];
	int i;

	snprintf(buf, sizeof(buf), "%s: fsm.scale (%%ld)", what);
	diff_eq_int(buf, ours->fsm.scale, ref->fsm.scale, tag);
	snprintf(buf, sizeof(buf), "%s: mrf.need (%%ld)", what);
	diff_eq_int(buf, ours->tx_mrf.need, ref->tx_mrf.need, tag);
	snprintf(buf, sizeof(buf), "%s: mrf.phase (%%ld)", what);
	diff_eq_int(buf, ours->tx_mrf.phase, ref->tx_mrf.phase, tag);
	snprintf(buf, sizeof(buf), "%s: mrf.widx (%%ld)", what);
	diff_eq_int(buf, ours->tx_mrf.widx, ref->tx_mrf.widx, tag);

	/* The resampler history, which is where a one-sample slip would hide. */
	snprintf(buf, sizeof(buf), "%s: mrf history[%%ld]", what);
	for (i = 0; i < ours->tx_mrf.history_len; i++)
		diff_eq_int(buf, ours->tx_mrf.history[i],
			    ref->tx_mrf.history[i], i);

	/* The 7200 Hz staging buffer, sample for sample. */
	snprintf(buf, sizeof(buf), "%s: scratch[%%ld]", what);
	for (i = 0; i < 162; i++)
		diff_eq_int(buf, ours->scratch[i], ref->scratch[i], i);
}

/*
 * Drive one call through both sides and compare everything observable.
 * `silent` selects TxNoCarrierB103 instead of ModDataB103.
 */
static void
tx_call(const char *what, struct b103fp *a, struct b103fp *b,
	const unsigned short *bits, int nbits, int silent, int tag)
{
	static short oa[512], ob[512];
	char buf[96];
	short na, nb;
	int i, nonzero = 0;

	memset(oa, 0x7e, sizeof(oa));
	memset(ob, 0x7e, sizeof(ob));

	if (silent) {
		na = ref_TxNoCarrierB103(a, bits, oa, (unsigned short)nbits);
		nb = TxNoCarrierB103(b, bits, ob, (unsigned short)nbits);
	} else {
		na = ref_ModDataB103(a, bits, oa, (unsigned short)nbits);
		nb = ModDataB103(b, bits, ob, (unsigned short)nbits);
	}

	snprintf(buf, sizeof(buf), "%s: sample count (%%ld)", what);
	diff_eq_int(buf, nb, na, tag);

	if (na >= 0 && na < 256)
		distinct_counts[na]++;

	snprintf(buf, sizeof(buf), "%s: sample[%%ld]", what);
	for (i = 0; i < na; i++) {
		diff_eq_int(buf, ob[i], oa[i], i);
		if (oa[i] != 0)
			nonzero++;
	}

	/* Nothing may be written past the reported count. */
	snprintf(buf, sizeof(buf), "%s: no write past count at [%%ld]", what);
	for (i = na; i < na + 8 && i < 512; i++)
		diff_eq_int(buf, ob[i], (short)0x7e7e, i);

	if (nonzero > 0)
		saw_nonzero_output++;
	else
		saw_silent_output++;

	compare_tx_state(what, b->dsp, a->dsp, tag);
}

/* Coverage for the receive chain. */
static int rx_states_seen[24];
static int rx_bits_seen;
static int rx_reset_seen;

/*
 * Compare everything DemodDataB103 can touch.  The two objects have separate
 * buffers, so the pointer fields differ and the contents are compared instead.
 */
static void
compare_rx(const char *what, struct b103_dsp *ours, struct b103_dsp *ref,
	   int tag)
{
	char buf[96];
	int i;

	snprintf(buf, sizeof(buf), "%s: rx_state (%%ld)", what);
	diff_eq_int(buf, ours->rx_state, ref->rx_state, tag);
	snprintf(buf, sizeof(buf), "%s: rx_energy (%%ld)", what);
	diff_eq_int(buf, ours->rx_energy, ref->rx_energy, tag);
	snprintf(buf, sizeof(buf), "%s: rx_tone (%%ld)", what);
	diff_eq_int(buf, ours->rx_tone, ref->rx_tone, tag);

	snprintf(buf, sizeof(buf), "%s: agc level (%%ld)", what);
	diff_eq_int(buf, ours->agc.level, ref->agc.level, tag);
	snprintf(buf, sizeof(buf), "%s: agc mult (%%ld)", what);
	diff_eq_int(buf, ours->agc.mult, ref->agc.mult, tag);
	snprintf(buf, sizeof(buf), "%s: agc shift (%%ld)", what);
	diff_eq_int(buf, ours->agc.shift, ref->agc.shift, tag);
	snprintf(buf, sizeof(buf), "%s: agc freeze (%%ld)", what);
	diff_eq_int(buf, ours->agc.freeze, ref->agc.freeze, tag);
	snprintf(buf, sizeof(buf), "%s: det_agc level (%%ld)", what);
	diff_eq_int(buf, ours->det_agc.level, ref->det_agc.level, tag);
	snprintf(buf, sizeof(buf), "%s: det_agc mult (%%ld)", what);
	diff_eq_int(buf, ours->det_agc.mult, ref->det_agc.mult, tag);

	snprintf(buf, sizeof(buf), "%s: rx_mrf phase (%%ld)", what);
	diff_eq_int(buf, ours->rx_mrf.phase, ref->rx_mrf.phase, tag);
	snprintf(buf, sizeof(buf), "%s: rx_mrf widx (%%ld)", what);
	diff_eq_int(buf, ours->rx_mrf.widx, ref->rx_mrf.widx, tag);
	snprintf(buf, sizeof(buf), "%s: rx_mrf hist[%%ld]", what);
	for (i = 0; i < ours->rx_mrf.history_len; i++)
		diff_eq_int(buf, ours->rx_mrf.history[i],
			    ref->rx_mrf.history[i], i);

	snprintf(buf, sizeof(buf), "%s: fsd bit (%%ld)", what);
	diff_eq_int(buf, ours->fsd.bit, ref->fsd.bit, tag);
	snprintf(buf, sizeof(buf), "%s: fsd since_bit (%%ld)", what);
	diff_eq_int(buf, ours->fsd.since_bit, ref->fsd.since_bit, tag);
	snprintf(buf, sizeof(buf), "%s: fsd hist_idx (%%ld)", what);
	diff_eq_int(buf, ours->fsd.hist_idx, ref->fsd.hist_idx, tag);

	snprintf(buf, sizeof(buf), "%s: scratch[%%ld]", what);
	for (i = 0; i < 48; i++)
		diff_eq_int(buf, ours->scratch[i], ref->scratch[i], i);
	snprintf(buf, sizeof(buf), "%s: rx_scratch[%%ld]", what);
	for (i = 0; i < 160; i++)
		diff_eq_int(buf, ours->rx_scratch[i], ref->rx_scratch[i], i);
}

/*
 * One receive call through both sides.  `in` is modified in place, so each
 * side gets its own copy and both copies are compared afterwards -- the mixer
 * writes back through that pointer and a divergence there would otherwise be
 * invisible.
 */
static void
rx_call(const char *what, struct b103fp *a, struct b103fp *b,
	const short *in, int n, int tag)
{
	static short ia[512], ib[512];
	static unsigned short ba[64], bb[64];
	char buf[96];
	short na, nb;
	int i;

	memcpy(ia, in, (unsigned)n * sizeof(short));
	memcpy(ib, in, (unsigned)n * sizeof(short));
	memset(ba, 0x5a, sizeof(ba));
	memset(bb, 0x5a, sizeof(bb));

	na = ref_DemodDataB103(a, ia, ba, (unsigned short)n);
	nb = DemodDataB103(b, ib, bb, (unsigned short)n);

	snprintf(buf, sizeof(buf), "%s: bit count (%%ld)", what);
	diff_eq_int(buf, nb, na, tag);

	snprintf(buf, sizeof(buf), "%s: mixed input[%%ld]", what);
	for (i = 0; i < n; i++)
		diff_eq_int(buf, ib[i], ia[i], i);

	snprintf(buf, sizeof(buf), "%s: bit[%%ld]", what);
	for (i = 0; i < na; i++) {
		diff_eq_int(buf, bb[i], ba[i], i);
		rx_bits_seen++;
	}

	if (a->dsp->rx_state >= 0 && a->dsp->rx_state < 24)
		rx_states_seen[a->dsp->rx_state]++;

	compare_rx(what, b->dsp, a->dsp, tag);
}

int
main(void)
{
	struct b103fp *a, *b;
	unsigned short bits[8];
	int rc = 0;
	int k, i;

	a = ref_B103FP_create(0, ref_B103_CFG);
	b = ref_B103FP_create(0, ref_B103_CFG);

	diff_begin("B103FP objects");
	diff_eq_int("reference built two objects (%ld)", a != 0 && b != 0, 1, 0);
	if (a == 0 || b == 0) {
		return diff_end();
	}
	diff_eq_int("they are distinct (%ld)", a != b, 1, 0);
	diff_eq_int("their dsp blocks are distinct (%ld)",
		    a->dsp != b->dsp, 1, 0);
	rc |= diff_end();

	/*
	 * A run of bit counts 1..6 -- six bits is what the 162-sample scratch
	 * buffer holds at 24 samples each -- cycled so the resampler's
	 * fractional remainder lands in a different place each time.
	 */
	diff_begin("ModDataB103");
	for (k = 0; k < 240; k++) {
		int n = (k % 6) + 1;

		for (i = 0; i < n; i++)
			bits[i] = (unsigned short)((k >> i) & 1);
		tx_call("ModData", a, b, bits, n, 0, k);
	}
	rc |= diff_end();

	/* A steady mark and a steady space, the two extremes of the FSK pair. */
	diff_begin("ModDataB103 steady tones");
	for (k = 0; k < 60; k++) {
		for (i = 0; i < 6; i++)
			bits[i] = (unsigned short)(k & 1);
		tx_call("steady", a, b, bits, 6, 0, k);
	}
	rc |= diff_end();

	/*
	 * TxNoCarrierB103.  Run it interleaved with ModDataB103 so that a
	 * version which forgot to restore the scale, or which muted the
	 * output buffer instead of the modulator, diverges on the call after.
	 *
	 * Three silent calls per signal call, because one is not enough to
	 * see the interesting part: the resampler's history still holds the
	 * tail of the previous carrier, so the FIRST silent block is not
	 * silent at all -- it is the carrier decaying through the filter.
	 * Only by the second or third does the output reach zero, and that
	 * is what proves the modulator is muted rather than the buffer.
	 */
	diff_begin("TxNoCarrierB103");
	for (k = 0; k < 120; k++) {
		int n = (k % 6) + 1;

		for (i = 0; i < n; i++)
			bits[i] = (unsigned short)((k >> i) & 1);
		tx_call("nocarrier", a, b, bits, n, 1, k);
		tx_call("nocarrier settled", a, b, bits, n, 1, k);
		tx_call("nocarrier settled 2", a, b, bits, n, 1, k);
		tx_call("after nocarrier", a, b, bits, n, 0, k);
	}
	rc |= diff_end();

	/*
	 * The claim that TxNoCarrier keeps the transmitter's timing: from the
	 * same starting state, silence and signal must advance the modulator
	 * and the resampler by exactly the same amount, and produce the same
	 * number of samples.
	 */
	diff_begin("TxNoCarrier keeps timing");
	{
		struct b103fp *c = ref_B103FP_create(0, ref_B103_CFG);
		struct b103fp *d = ref_B103FP_create(0, ref_B103_CFG);
		static short oc[512], od[512];
		short nc, nd;

		if (c != 0 && d != 0) {
			for (i = 0; i < 6; i++)
				bits[i] = (unsigned short)(i & 1);

			nc = ref_ModDataB103(c, bits, oc, 6);
			nd = ref_TxNoCarrierB103(d, bits, od, 6);

			diff_eq_int("same sample count (%ld)", nd, nc, 0);
			diff_eq_int("same mrf phase (%ld)",
				    d->dsp->tx_mrf.phase,
				    c->dsp->tx_mrf.phase, 0);
			diff_eq_int("same mrf widx (%ld)",
				    d->dsp->tx_mrf.widx,
				    c->dsp->tx_mrf.widx, 0);
			diff_eq_int("scale restored (%ld)",
				    d->dsp->fsm.scale,
				    c->dsp->fsm.scale, 0);

			/* And it really was silent, unlike the other. */
			for (i = 0, k = 0; i < nd; i++)
				if (od[i] != 0)
					k++;
			diff_eq_int("no-carrier output is silent (%ld)", k, 0, 0);
			for (i = 0, k = 0; i < nc; i++)
				if (oc[i] != 0)
					k++;
			diff_eq_int("carrier output is not (%ld)", k > 0, 1, k);

			ref_B103FP_delete(c);
			ref_B103FP_delete(d);
		}
	}
	rc |= diff_end();

	/* CarrierDetectB103: all four flag combinations. */
	diff_begin("CarrierDetectB103");
	for (k = 0; k < 4; k++) {
		a->dsp->rx_energy = b->dsp->rx_energy = (k & 1);
		a->dsp->rx_tone = b->dsp->rx_tone = ((k >> 1) & 1);
		diff_eq_int("flags %ld", CarrierDetectB103(b),
			    ref_CarrierDetectB103(a), k);
	}
	/* Non-boolean values, since the original ANDs bitwise. */
	a->dsp->rx_energy = b->dsp->rx_energy = 6;
	a->dsp->rx_tone = b->dsp->rx_tone = 3;
	diff_eq_int("bitwise not logical (%ld)", CarrierDetectB103(b),
		    ref_CarrierDetectB103(a), 0);
	diff_eq_int("6 & 3 is 2, not 1 (%ld)", ref_CarrierDetectB103(a), 2, 0);
	rc |= diff_end();

	/*
	 * Anti-vacuity.  Without these the run above proves only that two
	 * implementations agreed on whatever they did.
	 */
	diff_begin("b103fp coverage");
	diff_eq_int("calls producing a signal (%ld)",
		    saw_nonzero_output > 0, 1, saw_nonzero_output);
	diff_eq_int("calls producing complete silence (%ld)",
		    saw_silent_output > 0, 1, saw_silent_output);
	for (i = 0, k = 0; i < 256; i++)
		if (distinct_counts[i])
			k++;
	diff_eq_int("distinct sample counts seen (%ld)", k > 1, 1, k);
	rc |= diff_end();

	printf("t_b103fp: %d signal calls, %d silent, %d distinct sample counts\n",
	       saw_nonzero_output, saw_silent_output, k);


	/* --- DemodDataB103 --------------------------------------------- */

	/*
	 * B103_CFG leaves hdx->tone_detect NULL -- the branch of
	 * B103FP_create that fills it is not reached with this config, the
	 * same way dsp[+0xf4]'s bandpass is not (finding 32).  Calling the
	 * acquisition path on such an object would dereference NULL in both
	 * implementations, so a real tone object is installed first.  That is
	 * a state the original supports; it is only the default config that
	 * does not build it.
	 */
	diff_begin("DemodDataB103");
	{
		struct b103fp *ra = ref_B103FP_create(0, ref_B103_CFG);
		struct b103fp *rb = ref_B103FP_create(0, ref_B103_CFG);
		struct b103fp *tx = ref_B103FP_create(0, ref_B103_CFG);
		static short air[512];
		int f, n8;

		if (ra && rb && tx) {
			ra->hdx->tone_detect =
				ref_FPM_TONE_create(0, ref_FPM_TONE_CFG);
			rb->hdx->tone_detect =
				ref_FPM_TONE_create(0, ref_FPM_TONE_CFG);

			/*
			 * The stimulus walks the acquisition state machine
			 * deliberately.  What it acquires on is the 2100 Hz
			 * answer tone -- Bell 103 FSK does not advance it,
			 * because the detector is a sharp bandpass at 2100
			 * (1900-2300 Hz) and reports NOSIGNAL outside that.
			 *
			 *   frames 0-1    2100 Hz   0 -> 5 -> 10
			 *   frame  2      silence   reset to 0, proving it can
			 *   frames 3-22   2100 Hz   climbs through 15 to 16
			 *   frames 23+    FSK       demodulated, since 16 parks
			 *
			 * Once the state reaches 16 the detector is never
			 * consulted again, so the FSK cannot undo it.
			 */
			for (f = 0; f < 150; f++) {
				if (f == 2 || (f > 100 && (f % 17) == 0)) {
					n8 = 160;
					for (i = 0; i < n8; i++)
						air[i] = 0;
				} else if (f < 23) {
					n8 = 160;
					for (i = 0; i < n8; i++) {
						double tt = (f * 160.0 + i)
							    / 8000.0;

						air[i] = (short)(12000.0 *
							sin(2.0 * 3.14159265358979
							    * 2100.0 * tt));
					}
				} else {
					for (i = 0; i < 6; i++)
						bits[i] = (unsigned short)
							((0x2d3u >> ((f * 6 + i) % 10)) & 1);
					n8 = ref_ModDataB103(tx, bits, air, 6);
				}
				rx_call("demod", ra, rb, air, n8, f);
			}

			ref_B103FP_delete(ra);
			ref_B103FP_delete(rb);
			ref_B103FP_delete(tx);
		} else {
			diff_eq_int("three objects built (%ld)", 0, 1, 0);
		}
	}
	rc |= diff_end();

	/*
	 * Anti-vacuity for the acquisition state machine.  It advances in
	 * steps of five and parks at 16, so seeing 0 and 16 is not enough --
	 * the intermediate states are where a mis-transcribed increment or a
	 * wrong comparison would show.
	 */
	diff_begin("DemodDataB103 coverage");
	for (i = 0, k = 0; i < 24; i++)
		if (rx_states_seen[i])
			k++;
	diff_eq_int("distinct rx_states reached (%ld)", k >= 4, 1, k);
	diff_eq_int("acquisition reached 5 (%ld)", rx_states_seen[5] > 0, 1,
		    rx_states_seen[5]);
	diff_eq_int("acquisition reached 10 (%ld)", rx_states_seen[10] > 0, 1,
		    rx_states_seen[10]);
	diff_eq_int("demodulating state 16 reached (%ld)",
		    rx_states_seen[16] > 0, 1, rx_states_seen[16]);
	diff_eq_int("acquisition reset to 0 (%ld)", rx_states_seen[0] > 0, 1,
		    rx_states_seen[0]);
	diff_eq_int("bits demodulated (%ld)", rx_bits_seen > 0, 1, rx_bits_seen);
	rc |= diff_end();
	(void)rx_reset_seen;

	printf("t_b103fp: rx states 0/5/10/15/16 seen %d/%d/%d/%d/%d, %d bits\n",
	       rx_states_seen[0], rx_states_seen[5], rx_states_seen[10],
	       rx_states_seen[15], rx_states_seen[16], rx_bits_seen);

	ref_B103FP_delete(a);
	ref_B103FP_delete(b);
	return rc;
}

/*
 * t_spandsp_v23.c -- V.23 against an independent implementation.
 *
 * Every differential test in this tree compares the reconstruction with the
 * blob, which establishes equivalence and nothing else: if the blob were
 * wrong, a bit-exact reconstruction would be wrong in the same way and all
 * 510 of them would still pass.  This one asks whether it is a correct V.23
 * modem, by talking to SpanDSP -- written from the standard, by someone else,
 * without reference to this object.
 *
 * That question is not academic here.  V.23's coefficient tables were read
 * out of the blob and the filters they describe were never independently
 * checked; bwchdem.c's mark resonator solves to 364 Hz against a nominal 390
 * and is documented as unexplained.  If that offset were a reconstruction
 * error rather than the original's, only a third party could say so.
 *
 * FOUR DIRECTIONS, because V.23 is asymmetric and each fails differently:
 *
 *   ch1 ours -> SpanDSP   the 1200 bps transmitter produces a signal a
 *                         standards implementation recognises
 *   ch1 SpanDSP -> ours   v23FP_rx accepts one -- the discriminator, the 3:4
 *                         resampler and the 5-samples-per-bit clock
 *   ch2 ours -> SpanDSP   the same transmitter, pointed at 390/450 with the
 *                         { 107, 107, 106 } table
 *   ch2 SpanDSP -> ours   BwChDem's two resonators, including the 364 Hz one
 *
 * If one direction fails and its opposite passes, that localises it: a
 * transmit failure whose receive works points at the rate or the bit-period
 * table, and a receive failure whose transmit works points at filter tuning
 * or the acquisition gate -- because our own transmitter would carry the same
 * error and the two would agree.
 *
 * THE HALVES, NOT THE COMPOSITE.  CreateV23Modem with the answer tone armed
 * emits three seconds of 2100 Hz, which is channel 1's SPACE frequency, and
 * SpanDSP would dutifully demodulate the answer tone as data.  So this drives
 * v23FP_tx / v23FP_rx / BwChDem directly, as t_spandsp_b103 drives
 * ModDataB103 rather than the datapump.
 *
 * LICENCE: SpanDSP is LGPL and this reconstruction is BSD.  SpanDSP is linked
 * only into this binary, is never a dependency of src/, and no SpanDSP code,
 * table or algorithm has been read into src/.  It is a black box here: audio
 * in, bits out.  See third_party/README.md.
 */

#include <math.h>
#include <stdio.h>
#include <string.h>

#include <spandsp.h>

#include "dsplib/v23fp.h"

#define MAXBITS 4000

/* The bit stream both directions agree to send, and what came back. */
static unsigned char sent[MAXBITS];
static int sent_int[MAXBITS];
static unsigned char got[MAXBITS];
static int nsent, ngot;

static int failures;
static int checks;

static void
check(const char *what, int ok, const char *detail)
{
	checks++;
	if (!ok) {
		failures++;
		printf("  FAIL %s: %s\n", what, detail);
	}
}

/* SpanDSP hands recovered bits back one at a time through this. */
static void
put_bit(void *user, int bit)
{
	(void)user;
	if (bit < 0)			/* status codes, not data */
		return;
	if (ngot < MAXBITS)
		got[ngot++] = (unsigned char)(bit & 1);
}

/* And asks for bits to send through this. */
static int
get_bit(void *user)
{
	(void)user;
	if (nsent >= MAXBITS)
		return 1;		/* idle mark past the end */
	return sent[nsent++];
}

/*
 * Line up two bit streams and count errors.
 *
 * `skip` is how much of the head to ignore, and it is a parameter rather than
 * a constant because the two channels need wildly different preambles: our
 * 1200 bps receiver wants about a second of continuous mark before its tone
 * detector opens the gate, and at 75 bps a second is 75 bits rather than
 * 1200.  A fixed skip would either throw away most of a channel-2 run or fail
 * to clear channel 1's acquisition.
 */
static double
bit_error_rate(const unsigned char *a, int na, const unsigned char *b, int nb,
	       int skip, int *lag_out, int *compared_out)
{
	int best_err = 1 << 30, best_lag = 0, best_n = 0;
	int lag;

	for (lag = -200; lag <= 200; lag++) {
		int err = 0, n = 0, i;

		for (i = skip; i < na - 50; i++) {
			int j = i + lag;

			if (j < 0 || j >= nb)
				continue;
			err += (a[i] != b[j]);
			n++;
		}
		if (n > 100 && err < best_err) {
			best_err = err;
			best_lag = lag;
			best_n = n;
		}
	}
	*lag_out = best_lag;
	*compared_out = best_n;
	return best_n ? (double)best_err / best_n : 1.0;
}

/*
 * When the two streams disagree, say HOW.  A bit error rate is a single
 * number and every interesting failure mode produces the same one: a clock
 * that runs slow gives a rising error density, a marginal slicer gives errors
 * scattered uniformly, and a receiver that mistakes short symbols gives them
 * clustered on isolated bits.  Printing the shape costs nothing and is the
 * difference between "V.23 channel 2 disagrees" and knowing which end to
 * look at.
 */
static void
error_shape(const unsigned char *a, int na, const unsigned char *b, int nb,
	    int skip, int lag)
{
	int shown = 0, errs = 0, run = 0, best_run = 0;
	int isolated = 0;		/* wrong bit, both neighbours right   */
	int first = -1, last = -1;
	int i;

	printf("    errors at:");
	for (i = skip; i < na - 50; i++) {
		int j = i + lag;
		int wrong;

		if (j < 0 || j >= nb)
			continue;
		wrong = a[i] != b[j];
		if (!wrong) {
			run++;
			if (run > best_run)
				best_run = run;
			continue;
		}
		run = 0;
		errs++;
		if (first < 0)
			first = i;
		last = i;
		/* Was this bit a run of one in the SENT stream? */
		if (i > 0 && i + 1 < na && a[i] != a[i - 1] && a[i] != a[i + 1])
			isolated++;
		if (shown < 24) {
			printf(" %d", i);
			shown++;
		}
	}
	printf("%s\n", shown < errs ? " ..." : "");
	printf("    %d errors, first %d, last %d, longest clean run %d, "
	       "%d of %d were isolated bits in the sent stream\n",
	       errs, first, last, best_run, isolated, errs);
}

/*
 * Build the bit stream: `idle` mark bits, then a maximal-length sequence.
 *
 * The idle run is not padding.  Neither of our receivers emits anything until
 * it has heard its channel's MARK frequency -- v23FP_rx wants two blocks in
 * which 88.5% of the energy is at 1300 Hz, BwChDem wants ten consecutive
 * blocks of 390 -- and pseudorandom data spends half its time at the space
 * frequency and passes neither gate.  A run of ones is what an idle V.23
 * channel actually carries, so this is the real acquisition condition rather
 * than a test convenience.
 */
static void
build_stream(int total, int idle)
{
	unsigned lfsr = 0x2B1Du;
	int i;

	for (i = 0; i < total; i++) {
		if (i < idle) {
			sent[i] = 1;
		} else {
			lfsr = (lfsr >> 1) ^ (-(int)(lfsr & 1u) & 0xB400u);
			sent[i] = (unsigned char)(lfsr & 1);
		}
		sent_int[i] = sent[i];
	}
}

static struct v23_cfg
config(void)
{
	struct v23_cfg cfg;

	memset(&cfg, 0, sizeof(cfg));
	cfg.answer_tone = 0;
	cfg.sample_rate = 8000;
	cfg.silence_limit = 1000000;	/* never give up mid-test */
	return cfg;
}

/* Root-mean-square of a block, so the transmit level is reported not assumed. */
static double
rms(const short *x, int n)
{
	double acc = 0.0;
	int i;

	for (i = 0; i < n; i++)
		acc += (double)x[i] * x[i];
	return n ? sqrt(acc / n) : 0.0;
}

/*
 * ---------------------------------------------------------------------------
 * Direction A: our transmitter -> SpanDSP's receiver.
 */
static void
ours_to_spandsp(const char *what, int preset, short mark, short space,
		const short *period, int total, int idle, int skip,
		int first_want)
{
	struct v23tx *tx;
	fsk_rx_state_t *rx;
	short air[160];
	int buf[64];
	double level = 0.0;
	int blocks = 0;
	int cursor = 0;
	int want = first_want;
	double ber;
	int lag, cmp;

	build_stream(total, idle);
	nsent = ngot = 0;
	memset(buf, 0, sizeof(buf));

	tx = v23FP_tx_create(NULL, mark, space, 3, period, 0);
	rx = fsk_rx_init(NULL, &preset_fsk_specs[preset], FSK_FRAME_MODE_SYNC,
			 put_bit, NULL);
	check(what, tx != NULL && rx != NULL, "init failed");
	if (tx == NULL || rx == NULL)
		return;

	/*
	 * THE CALLER PROTOCOL, and it is not the obvious one.
	 *
	 * `consumed` counts bits the transmitter FINISHED, and a call almost
	 * always ends part way through a bit -- the object keeps that one in
	 * `held` and does not read it from the buffer again.  So a caller that
	 * advances a cursor into a long array by `consumed` re-supplies the
	 * held bit, and the transmitter sends it twice: once from `held` and
	 * once as the next bit to start.
	 *
	 * v23_process does not do that.  It refills the buffer FROM INDEX ZERO
	 * with `consumed` fresh bits every block, so `buf[0]` is always the
	 * next bit that has never been handed over, which is exactly what the
	 * transmitter reads after finishing a held one.  This emulates that,
	 * including keeping the buffer across calls so stale entries persist
	 * as the real one's do.
	 *
	 * The distinction is invisible on the forward channel, where 160
	 * samples is exactly eight cycles of { 7, 7, 6 } and no bit is ever
	 * held.  On the backward channel it is 320 samples per cycle, so every
	 * block ends mid-bit and getting it wrong corrupts every third bit --
	 * which is how this comment came to be written.
	 */
	while (cursor + 8 < total) {
		int used = 0;
		int i;

		for (i = 0; i < want && cursor + i < total; i++)
			buf[i] = sent_int[cursor + i];
		cursor += want;

		v23FP_tx_progress(tx, air, 160, buf, &used);
		want = used > 0 ? used : 1;
		if (want > 64)
			want = 64;
		level += rms(air, 160);
		blocks++;
		fsk_rx(rx, air, 160);
	}
	nsent = cursor < total ? cursor : total;

	ber = bit_error_rate(sent, nsent, got, ngot, skip, &lag, &cmp);
	printf("  %s: %d bits sent, %d received, lag %d, %d compared, "
	       "BER %.5f, tx RMS %.0f\n", what, nsent, ngot, lag, cmp, ber,
	       blocks ? level / blocks : 0.0);
	check(what, ngot > total / 2,
	      "SpanDSP recovered almost nothing -- the transmitted signal is "
	      "not what this channel should carry");
	check(what, cmp > 100, "too little overlap to judge");
	if (ber > 0.0)
		error_shape(sent, nsent, got, ngot, skip, lag);
	check(what, ber < 0.001, "SpanDSP disagrees with what we sent");

	v23FP_tx_delete(tx);
	fsk_rx_free(rx);
}

/*
 * ---------------------------------------------------------------------------
 * Direction B: SpanDSP's transmitter -> our receiver.
 *
 * The two receivers do not share a signature and must be driven separately,
 * so `forward` selects which.  Blocks are 160 samples for both: v23FP_rx's
 * det_buf is exactly that long and nothing in it bounds the copy, so a larger
 * block would run off the end of the object.
 */
static void
spandsp_to_ours(const char *what, int preset, int forward, int total,
		int skip)
{
	struct v23_cfg cfg = config();
	fsk_tx_state_t *tx;
	struct v23rx *rxf = NULL;
	struct bwchdem *rxb = NULL;
	short air[160];
	double level = 0.0;
	int blocks = 0;
	double ber;
	int lag, cmp;

	build_stream(total, skip);
	nsent = ngot = 0;

	tx = fsk_tx_init(NULL, &preset_fsk_specs[preset], get_bit, NULL);
	if (forward)
		rxf = v23FP_rx_create(NULL, &cfg);
	else
		rxb = BwChDem_Create(NULL, &cfg);
	check(what, tx != NULL && (rxf != NULL || rxb != NULL), "init failed");
	if (tx == NULL || (rxf == NULL && rxb == NULL))
		return;

	while (nsent < total && ngot < MAXBITS) {
		int bits[64];
		int nb = 0;		/* neither receiver writes it on 1 or 2 */
		int n, i;

		n = fsk_tx(tx, air, 160);
		if (n <= 0)
			break;
		level += rms(air, n);
		blocks++;

		if (forward)
			v23FP_rx_progress(rxf, air, n, bits, &nb);
		else
			BwChDem_Progress(rxb, air, (short)n, bits, &nb);

		for (i = 0; i < nb && ngot < MAXBITS; i++)
			got[ngot++] = (unsigned char)(bits[i] & 1);
	}

	ber = bit_error_rate(sent, nsent, got, ngot, skip, &lag, &cmp);
	printf("  %s: %d bits sent, %d received, lag %d, %d compared, "
	       "BER %.5f, rx RMS %.0f\n", what, nsent, ngot, lag, cmp, ber,
	       blocks ? level / blocks : 0.0);
	check(what, ngot > total / 2,
	      "we recovered almost nothing from a standards-conformant signal");
	check(what, cmp > 100, "too little overlap to judge");
	if (ber > 0.0)
		error_shape(sent, nsent, got, ngot, skip, lag);
	check(what, ber < 0.001, "we disagree with what SpanDSP sent");

	if (rxf)
		v23FP_rx_delete(rxf);
	if (rxb)
		BwChDem_Delete(rxb);
	fsk_tx_free(tx);
}

int
main(void)
{
	static const short fw_period[3] = { 7, 7, 6 };
	static const short bw_period[3] = { 107, 107, 106 };

	printf("SpanDSP interop: V.23\n");

	/*
	 * Assert the presets before using them.  A wrong SpanDSP version would
	 * otherwise fail deep in the bit comparison with a symptom that looks
	 * like a polarity bug in the reconstruction and is not one -- exactly
	 * the trap t_spandsp_b103 documents for Bell 103's swapped channels.
	 *
	 * Note also that both channels agree with V.23's own polarity: the
	 * ONE bit is the mark and the mark is the lower frequency in channel 1
	 * and the lower in channel 2 as well.  Nothing here has to be
	 * inverted, which is worth checking rather than discovering.
	 */
	check("V23CH1 is 1300 mark / 2100 space",
	      preset_fsk_specs[FSK_V23CH1].freq_one == 1300
	      && preset_fsk_specs[FSK_V23CH1].freq_zero == 2100,
	      "channel 1 is not 1300/2100 -- wrong SpanDSP version.  Build "
	      "against third_party/spandsp");
	check("V23CH1 runs at 1200 bps",
	      preset_fsk_specs[FSK_V23CH1].baud_rate == 1200 * 100,
	      "channel 1 is not 1200 bps");
	check("V23CH2 is 390 mark / 450 space",
	      preset_fsk_specs[FSK_V23CH2].freq_one == 390
	      && preset_fsk_specs[FSK_V23CH2].freq_zero == 450,
	      "channel 2 is not 390/450");
	check("V23CH2 runs at 75 bps",
	      preset_fsk_specs[FSK_V23CH2].baud_rate == 75 * 100,
	      "channel 2 is not 75 bps");

	/*
	 * Channel 1, 1200 bps.  1200 idle bits is one second of mark, which is
	 * what v23FP_rx's detector needs; the comparison skips a little past
	 * that so the settling either side of the boundary is not counted.
	 */
	ours_to_spandsp("ch1 ours -> SpanDSP", FSK_V23CH1, 1300, 2100,
			fw_period, 3000, 1200, 1400, 26);
	spandsp_to_ours("ch1 SpanDSP -> ours", FSK_V23CH1, 1, 3000, 1400);

	/*
	 * Channel 2, 75 bps -- sixteen times slower, so the same wall-clock
	 * costs sixteen times fewer bits.  150 idle bits is two seconds, far
	 * more than BwChDem's ten-block gate needs, and 900 total leaves 750
	 * to judge on.
	 */
	ours_to_spandsp("ch2 ours -> SpanDSP", FSK_V23CH2, 390, 450,
			bw_period, 900, 150, 200, 3);
	spandsp_to_ours("ch2 SpanDSP -> ours", FSK_V23CH2, 0, 900, 200);

	printf("%s: %d checks, %d failures\n", failures ? "FAIL" : "PASS",
	       checks, failures);
	return failures != 0;
}

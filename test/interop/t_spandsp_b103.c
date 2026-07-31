/*
 * t_spandsp_b103.c -- Bell 103 against an independent implementation.
 *
 * Every other test in this tree compares the reconstruction with the blob.
 * That establishes equivalence and nothing more: if the blob were wrong, a
 * bit-exact reconstruction would be wrong in the same way and every test
 * would still pass.  This one asks a different question -- is it a correct
 * Bell 103 modem -- by talking to SpanDSP, which was written from the
 * standard by someone else.
 *
 * Two directions, because they fail differently:
 *
 *   ours -> SpanDSP   proves the TRANSMITTER produces a signal a standards
 *                     implementation recognises.  A wrong tone, a wrong baud
 *                     rate or an inverted mark/space shows up here.
 *   SpanDSP -> ours   proves the RECEIVER accepts one.  A mis-tuned filter or
 *                     a wrong oscillator shows up here and nowhere else,
 *                     because our own transmitter would have the same error
 *                     and the two would agree.
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

#include "dsplib/b103fp.h"

#define NBITS 3000

static unsigned char sent[NBITS];
static unsigned char got[NBITS];
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

static struct b103fp *
make(int call_type)
{
	struct b103_cfg cfg = B103_CFG_data;

	cfg.call_type = call_type;
	return B103FP_create(NULL, &cfg);
}

/* SpanDSP hands recovered bits back one at a time through this. */
static void
put_bit(void *user, int bit)
{
	(void)user;
	if (bit < 0)			/* PUTBIT_* status codes, not data */
		return;
	if (ngot < NBITS)
		got[ngot++] = (unsigned char)(bit & 1);
}

/* And asks for bits to send through this. */
static int
get_bit(void *user)
{
	(void)user;
	if (nsent >= NBITS)
		return 0;
	return sent[nsent++];
}

/*
 * Line up two bit streams and count errors.  The pipelines have different
 * latencies, so the alignment is searched rather than assumed.
 */
static double
bit_error_rate(const unsigned char *a, int na, const unsigned char *b, int nb,
	       int *lag_out, int *compared_out)
{
	int best_err = 1 << 30, best_lag = 0, best_n = 0;
	int lag;

	for (lag = -200; lag <= 200; lag++) {
		int err = 0, n = 0, i;

		for (i = 100; i < na - 50; i++) {
			int j = i + lag;

			if (j < 0 || j >= nb)
				continue;
			err += (a[i] != b[j]);
			n++;
		}
		if (n > 400 && err < best_err) {
			best_err = err;
			best_lag = lag;
			best_n = n;
		}
	}
	*lag_out = best_lag;
	*compared_out = best_n;
	return best_n ? (double)best_err / best_n : 1.0;
}

int
main(void)
{
	unsigned lfsr = 0x2B1Du;
	int i;

	printf("SpanDSP %s\n", spandsp_version());

	for (i = 0; i < NBITS; i++) {
		lfsr = (lfsr >> 1) ^ (-(int)(lfsr & 1u) & 0xB400u);
		sent[i] = (unsigned char)(lfsr & 1);
	}

	/*
	 * 1. Our originating transmitter -> SpanDSP's Bell 103 channel 1
	 *    receiver.  Channel 1 is 1070/1270, which is what an originating
	 *    station sends.
	 */
	{
		struct b103fp *tx = make(B103_CALL_ORIGINATE);
		fsk_rx_state_t *rx;
		unsigned short bits[8];
		short air[512];
		int f, n, k = 0;
		double ber;
		int lag, cmp;

		nsent = ngot = 0;
		rx = fsk_rx_init(NULL, &preset_fsk_specs[FSK_BELL103CH1],
				 FSK_FRAME_MODE_ASYNC, put_bit, NULL);
		check("spandsp rx init", rx != NULL && tx != NULL, "init failed");

		if (rx && tx) {
			for (f = 0; k + 6 < NBITS; f++) {
				for (i = 0; i < 6; i++)
					bits[i] = sent[k++];
				n = ModDataB103(tx, bits, air, 6);
				fsk_rx(rx, air, n);
			}
			nsent = k;
			ber = bit_error_rate(sent, nsent, got, ngot, &lag, &cmp);
			printf("  ours -> SpanDSP: %d bits sent, %d received, "
			       "lag %d, %d compared, BER %.5f\n",
			       nsent, ngot, lag, cmp, ber);
			check("ours -> SpanDSP recovered bits", ngot > 1000,
			      "SpanDSP recovered almost nothing -- the "
			      "transmitted signal is not Bell 103 channel 1");
			check("ours -> SpanDSP error-free", ber < 0.001,
			      "SpanDSP disagrees with what we sent");
		}
		if (tx)
			B103FP_delete(tx);
	}

	/*
	 * 2. SpanDSP's Bell 103 channel 2 transmitter -> our answering
	 *    receiver.  Channel 2 is 2025/2225 -- what an ANSWERING station
	 *    sends, and therefore what an originating station receives.
	 */
	{
		struct b103fp *rx = make(B103_CALL_ORIGINATE);
		fsk_tx_state_t *tx;
		unsigned short out[64];
		short air[512];
		int total = 0, nb;
		double ber;
		int lag, cmp;

		nsent = ngot = 0;
		tx = fsk_tx_init(NULL, &preset_fsk_specs[FSK_BELL103CH2],
				 get_bit, NULL);
		check("spandsp tx init", tx != NULL && rx != NULL, "init failed");

		if (tx && rx) {
			/* Drop straight into the data state; the handshake is
			 * exercised by t_b103hdx, not here. */
			rx->hdx->substate = B103_STATE_WAIT1;
			B103NextState[rx->hdx->mode](rx);
			rx->dsp->rx_state = 16;

			while (nsent < NBITS - 100 && total < NBITS - 100) {
				int n = fsk_tx(tx, air, 160);

				if (n <= 0)
					break;
				nb = DemodDataB103(rx, air, out,
						   (unsigned short)n);
				for (i = 0; i < nb && total < NBITS; i++)
					got[total++] = (unsigned char)out[i];
			}
			ngot = total;
			ber = bit_error_rate(sent, nsent, got, ngot, &lag, &cmp);
			printf("  SpanDSP -> ours: %d bits sent, %d received, "
			       "lag %d, %d compared, BER %.5f\n",
			       nsent, ngot, lag, cmp, ber);
			check("SpanDSP -> ours recovered bits", ngot > 1000,
			      "we recovered almost nothing from a standards "
			      "implementation's Bell 103 channel 2");
			check("SpanDSP -> ours error-free", ber < 0.001,
			      "we disagree with what SpanDSP sent");
		}
		if (rx)
			B103FP_delete(rx);
	}

	printf("%s spandsp b103 interop   %d checks\n",
	       failures ? "FAIL" : "PASS", checks);
	return failures ? 1 : 0;
}

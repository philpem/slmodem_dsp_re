/*
 * t_spandsp_replay.c -- does the BLOB lose lock on SpanDSP's signal too?
 *
 * Finding 40: the receiver demodulates a SpanDSP Bell 103 signal correctly
 * for about 229 bits and then loses bit-clock lock, at some input levels and
 * not others.  Every module on that path is bit-exact with the blob, so the
 * blob should behave identically -- but "should" is an inference, and this is
 * the measurement.
 *
 * The interop tier cannot answer it: SpanDSP is 64-bit and the blob is
 * 32-bit, so they cannot share a process.  gen_spandsp_capture writes the
 * signal to a file and this replays it through both implementations in the
 * differential harness, where the blob is available.
 *
 * Two questions, and they are different:
 *
 *   1. Do the two implementations AGREE?  If they diverge, the bug is ours
 *      and the bit-exactness tests have a coverage hole.
 *   2. Does the ORIGINAL lose lock?  If it does, and they agree, then the
 *      behaviour is the blob's and the question moves to whether it matters.
 *
 * Skipped with a clear message if the capture is absent, so the suite still
 * runs for anyone who has not built SpanDSP.
 */

#include <stdio.h>
#include <stdlib.h>
#include <string.h>

#include "harness.h"
#include "dsplib/b103fp.h"

extern struct b103fp *ref_B103FP_create(struct b103fp *state, const void *cfg);
extern void ref_B103FP_delete(struct b103fp *state);
extern short ref_DemodDataB103(void *fp, short *in, unsigned short *bits,
			       unsigned short count);
extern void (*ref_B103NextState[3])(void *fp);

#define MAXSAMP 120000
#define MAXBITS 6000

static short pcm[MAXSAMP];
static unsigned char txbits[MAXBITS];
static int nsamp, ntx;

static unsigned char got_a[MAXBITS], got_b[MAXBITS];

static int
load(void)
{
	FILE *fp;

	fp = fopen("build/capture/spandsp_b103.pcm", "rb");
	if (fp == NULL)
		return 0;
	nsamp = (int)fread(pcm, sizeof(short), MAXSAMP, fp);
	fclose(fp);

	fp = fopen("build/capture/spandsp_b103.bits", "rb");
	if (fp == NULL)
		return 0;
	ntx = (int)fread(txbits, 1, MAXBITS, fp);
	fclose(fp);
	return nsamp > 1000 && ntx > 500;
}

static struct b103fp *
make_rx(void)
{
	struct b103_cfg cfg = B103_CFG_data;
	struct b103fp *fp;

	cfg.call_type = B103_CALL_ORIGINATE;
	fp = ref_B103FP_create(0, &cfg);
	if (fp == NULL)
		return NULL;
	fp->hdx->substate = B103_STATE_WAIT1;
	ref_B103NextState[fp->hdx->mode](fp);
	fp->dsp->rx_state = 16;
	return fp;
}

/* Correct bits from the start at a fixed alignment. */
static int
clean_prefix(const unsigned char *b, int nb, int lag)
{
	int i;

	for (i = 0; i < ntx && i + lag < nb; i++)
		if (txbits[i] != b[i + lag])
			return i;
	return i;
}

static int
best_lag(const unsigned char *b, int nb)
{
	int best = -1, bestn = -1, lag;

	for (lag = 0; lag < 40; lag++) {
		int n = clean_prefix(b, nb, lag);

		if (n > bestn) {
			bestn = n;
			best = lag;
		}
	}
	return best;
}

int
main(void)
{
	struct b103fp *a, *b;
	static short in_a[256], in_b[256];
	static unsigned short out_a[64], out_b[64];
	int rc = 0;
	int na = 0, nb = 0;
	int off, i, lag_a, lag_b, clean_a, clean_b;

	if (!load()) {
		printf("SKIP spandsp replay      "
		       "build/capture/spandsp_b103.pcm missing -- "
		       "run `make capture` (needs third_party/spandsp)\n");
		return 0;
	}

	a = make_rx();		/* driven by the reconstruction */
	b = make_rx();		/* driven by the blob           */

	diff_begin("spandsp replay: blob vs reconstruction");
	diff_eq_int("both receivers built (%ld)", a != 0 && b != 0, 1, 0);
	if (a == 0 || b == 0)
		return diff_end();

	for (off = 0; off + 160 <= nsamp; off += 160) {
		short ra, rb;

		memcpy(in_a, pcm + off, 160 * sizeof(short));
		memcpy(in_b, pcm + off, 160 * sizeof(short));

		rb = ref_DemodDataB103(b, in_b, out_b, 160);
		ra = DemodDataB103(a, in_a, out_a, 160);

		diff_eq_int("block %ld: bit count", ra, rb, off / 160);
		for (i = 0; i < 160; i++)
			diff_eq_int("block: mixed sample[%ld]", in_a[i],
				    in_b[i], i);
		for (i = 0; i < rb; i++) {
			diff_eq_int("block: bit[%ld]", out_a[i], out_b[i], i);
			if (na < MAXBITS)
				got_a[na++] = (unsigned char)out_a[i];
			if (nb < MAXBITS)
				got_b[nb++] = (unsigned char)out_b[i];
		}
	}
	rc |= diff_end();

	/*
	 * Now the second question.  The comparison above says whether they
	 * agree; this says what they DO.
	 */
	diff_begin("spandsp replay: does the original lose lock?");
	lag_a = best_lag(got_a, na);
	lag_b = best_lag(got_b, nb);
	clean_a = clean_prefix(got_a, na, lag_a);
	clean_b = clean_prefix(got_b, nb, lag_b);

	printf("  reconstruction: %d bits, best lag %d, clean for %d\n",
	       na, lag_a, clean_a);
	printf("  blob          : %d bits, best lag %d, clean for %d\n",
	       nb, lag_b, clean_b);
	printf("  (of %d transmitted)\n", ntx);

	diff_eq_int("same number of bits (%ld)", na, nb, 0);
	diff_eq_int("same alignment (%ld)", lag_a, lag_b, 0);
	diff_eq_int("same clean prefix (%ld)", clean_a, clean_b, 0);
	rc |= diff_end();

	/*
	 * The mechanism.  Lock is lost because the AGC's gain goes to ZERO
	 * for one block, silencing it -- and the gain goes to zero because
	 * FPM_div returns a zero reciprocal for that block's level estimate.
	 * That is D4, the out-of-range table read, firing in earnest.
	 *
	 * Replayed here rather than inferred: the whole capture is run again
	 * and every block whose AGC gain came out zero is counted.
	 */
	diff_begin("spandsp replay: the mechanism is D4");
	{
		struct b103fp *c = make_rx();
		static short in[256];
		static unsigned short out[64];
		int zero_gain = 0, first_zero = -1, blocks = 0;

		if (c != 0) {
			for (off = 0; off + 160 <= nsamp; off += 160) {
				memcpy(in, pcm + off, 160 * sizeof(short));
				ref_DemodDataB103(c, in, out, 160);
				if (c->dsp->agc.mult == 0) {
					zero_gain++;
					if (first_zero < 0)
						first_zero = blocks;
				}
				blocks++;
			}
			printf("  AGC gain hit zero on %d of %d blocks, "
			       "first at block %d\n",
			       zero_gain, blocks, first_zero);
			printf("  (its level estimate normalises to mantissa "
			       "0xff80, index 128 -- one past the table)\n");

			diff_eq_int("the AGC gain reaches zero (%ld)",
				    zero_gain > 0, 1, zero_gain);
			/*
			 * The first zero must coincide with the loss of lock.
			 * At six bits a block, bit 229 is block 38.
			 */
			diff_eq_int("it happens where lock is lost (%ld)",
				    first_zero, clean_a / 6, 0);
			ref_B103FP_delete(c);
		}
	}
	rc |= diff_end();

	ref_B103FP_delete(a);
	ref_B103FP_delete(b);
	return rc;
}

/*
 * t_fpm_mrf_filter.c -- differential test of the polyphase resampling core.
 *
 * `phase`, the history write index and `need` all persist across calls, so
 * the fragment-boundary behaviour is the point of this test rather than an
 * afterthought.  A reconstruction that recomputed `need` per call would agree
 * perfectly under bulk feeding and drift only when fragments are uneven --
 * which is exactly how the RcFixed output-limit bug hid.
 *
 * So every configuration is driven three ways: one big call, uneven chunks,
 * and one sample at a time.  The last is the harshest: with Bell 103's 10:9
 * transmit filter, `need` is 1 most of the time, so single-sample calls
 * repeatedly cross the partial-consume path.
 */

#include <string.h>

#include "harness.h"
#include "dsplib/fpm_mrf.h"

extern void ref_FPM_MRF_init(void *state, const void *cfg, int fresh);
extern void ref_FPM_MRF_free(void *state);
extern short ref_FPM_MRF_filter(void *state, const short *in, short *out,
				short count);
extern short ref_B103_MRF_FILT_TX[];
extern short ref_B103_MRF_FILT_RX[];

#define NSAMP 3000
static short input[NSAMP];
static short oa[NSAMP * 4], ob[NSAMP * 4];

static void
make_input(void)
{
	unsigned lfsr = 0x7A5B3Cu;
	int i;

	for (i = 0; i < NSAMP; i++) {
		lfsr = (lfsr >> 1) ^ (-(int)(lfsr & 1u) & 0xB400u);
		input[i] = (short)((int)(lfsr & 0x3fff) - 0x2000
				   + 5000 * ((i % 24) < 12 ? 1 : -1));
	}
}

static int
run(const char *label, short branches, short decimate, short *coeff,
    short taps, const int *chunks, int nchunks)
{
	struct fpm_mrf a, b;
	struct fpm_mrf_cfg cfg;
	int pos = 0, na = 0, nb = 0, ci = 0, rc;

	memset(&cfg, 0, sizeof(cfg));
	cfg.branches = branches;
	cfg.decimate = decimate;
	cfg.coeff = coeff;
	cfg.taps = taps;

	memset(&a, 0, sizeof(a));
	memset(&b, 0, sizeof(b));
	ref_FPM_MRF_init(&a, &cfg, 1);
	FPM_MRF_init(&b, &cfg, 1);

	diff_begin(label);
	while (pos < NSAMP) {
		int n = chunks[ci++ % nchunks];
		short ca, cb;
		int k;

		if (pos + n > NSAMP)
			n = NSAMP - pos;

		ca = ref_FPM_MRF_filter(&a, input + pos, oa + na, (short)n);
		cb = FPM_MRF_filter(&b, input + pos, ob + nb, (short)n);

		diff_eq_int("at %ld: output count", cb, ca, pos);
		if (ca != cb)
			break;
		for (k = 0; k < ca; k++)
			diff_eq_int("sample %ld", ob[nb + k], oa[na + k],
				    na + k);

		/* Carried state must match too, not just the audio. */
		diff_eq_int("at %ld: phase", b.phase, a.phase, pos);
		diff_eq_int("at %ld: widx", b.widx, a.widx, pos);
		diff_eq_int("at %ld: need", b.need, a.need, pos);

		na += ca;
		nb += cb;
		pos += n;
	}
	diff_eq_int("total outputs (%ld)", nb, na, 0);

	rc = diff_end();
	ref_FPM_MRF_free(&a);
	FPM_MRF_free(&b);
	return rc;
}

int
main(void)
{
	static const int bulk[] = { 480 };
	static const int ragged[] = { 7, 1, 53, 2, 160, 11, 3, 97 };
	static const int tiny[] = { 1 };
	int rc = 0;

	make_input();

	/* Bell 103 transmit, 7200 -> 8000. */
	rc |= run("MRF 10:9 bulk", 10, 9, ref_B103_MRF_FILT_TX, 270, bulk, 1);
	rc |= run("MRF 10:9 ragged", 10, 9, ref_B103_MRF_FILT_TX, 270, ragged,
		  sizeof(ragged) / sizeof(ragged[0]));
	rc |= run("MRF 10:9 one-at-a-time", 10, 9, ref_B103_MRF_FILT_TX, 270,
		  tiny, 1);

	/* Bell 103 receive, 8000 -> 2400: decimating, so `need` is usually 3. */
	rc |= run("MRF 3:10 bulk", 3, 10, ref_B103_MRF_FILT_RX, 90, bulk, 1);
	rc |= run("MRF 3:10 ragged", 3, 10, ref_B103_MRF_FILT_RX, 90, ragged,
		  sizeof(ragged) / sizeof(ragged[0]));
	rc |= run("MRF 3:10 one-at-a-time", 3, 10, ref_B103_MRF_FILT_RX, 90,
		  tiny, 1);

	return rc;
}

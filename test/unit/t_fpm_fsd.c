/*
 * t_fpm_fsd.c -- differential test of the demodulator's setup.
 *
 * Driven with Bell 103's real configuration, read out of B103FP_create: the
 * 15-tap channel filter and the IIR lowpass, with the same scalars.
 *
 * Buffer pointers differ between builds, so they are compared by length and
 * contents.  Both the fresh-allocate and re-init-in-place paths are covered.
 */

#include <string.h>

#include "harness.h"
#include "dsplib/fpm_fsd.h"
#include "dsplib/b103fp.h"

extern void ref_FPM_FSD_init(void *state, const void *cfg, int fresh);
extern void ref_FPM_FSD_free(void *state);
extern short ref_B103_CHAN_INTRP[];
extern short ref_B103_IIR_LPF[];
extern short ref_FPM_FSD_demodulate(void *state, const short *samples,
				    unsigned short *bits_out,
				    unsigned short count);
extern void *ref_B103FP_create(void *state, const void *cfg);
extern void ref_B103FP_delete(void *state);
extern short ref_B103_CFG[];
extern short ref_ModDataB103(void *fp, const unsigned short *bits, short *out,
			     unsigned short nbits);
extern short ref_FPM_MRF_filter(void *state, const short *in, short *out,
				short count);
extern void ref_FPM_AGC_agc(void *agc, short *samples, unsigned short count);

static void
compare(const struct fpm_fsd *ours, const struct fpm_fsd *ref)
{
	int i;

	diff_eq_int("fir_taps (%ld)", ours->cfg.fir_taps, ref->cfg.fir_taps, 0);
	diff_eq_int("iir_len (%ld)", ours->cfg.iir_len, ref->cfg.iir_len, 0);
	diff_eq_int("trace_len (%ld)", ours->cfg.trace_len, ref->cfg.trace_len, 0);
	diff_eq_int("last_count (%ld)", ours->last_count, ref->last_count, 0);
	diff_eq_int("f22 = f12/2 (%ld)", ours->f22, ref->f22, 0);
	diff_eq_int("hist_idx (%ld)", ours->hist_idx, ref->hist_idx, 0);
	diff_eq_int("bit (%ld)", ours->bit, ref->bit, 0);
	diff_eq_int("since_bit (%ld)", ours->since_bit, ref->since_bit, 0);
	diff_eq_int("disagreements (%ld)", ours->disagreements, ref->disagreements, 0);
	diff_eq_int("fir pointer copied (%ld)",
		    ours->cfg.fir == ref->cfg.fir, 1, 0);
	diff_eq_int("iir pointer copied (%ld)",
		    ours->cfg.iir == ref->cfg.iir, 1, 0);

	for (i = 0; i < ours->cfg.fir_taps; i++)
		diff_eq_int("fir_hist[%ld]", ours->fir_hist[i],
			    ref->fir_hist[i], i);
	for (i = 0; i < 2 * ours->cfg.iir_len; i++)
		diff_eq_int("iir_hist[%ld]", ours->iir_hist[i],
			    ref->iir_hist[i], i);
	for (i = 0; i < ours->cfg.trace_len; i++)
		diff_eq_int("trace[%ld]", ours->trace[i], ref->trace[i], i);
}

/*
 * ---------------------------------------------------------------------------
 * FPM_FSD_demodulate, driven by a real Bell 103 signal.
 *
 * The stimulus is not synthesised: ModDataB103 modulates a known bit pattern
 * into genuine 8 kHz FSK, and it then goes through the same front end
 * DemodDataB103 uses -- the 3:10 rate converter down to 2400 Hz, then the
 * AGC -- before reaching the demodulator.  That matters because a
 * differential test can pass with both sides emitting nothing if the input
 * never carried a signal, and hand-rolled tones at the wrong level or the
 * wrong rate are an easy way to arrange exactly that.
 *
 * Both sides are fed byte-identical samples: the front end runs once, on the
 * reference, and its output goes to both demodulators.  Only the demodulator
 * state is duplicated, so any divergence is the demodulator's.
 */
static int bits_emitted;
static int saw_bit[2];
static int saw_resync;
static int saw_freerun;
static int saw_cap_hit;

static void
compare_demod(const char *what, const struct fpm_fsd *ours,
	      const struct fpm_fsd *ref, int tag)
{
	char buf[96];
	int i;

	snprintf(buf, sizeof(buf), "%s: bit (%%ld)", what);
	diff_eq_int(buf, ours->bit, ref->bit, tag);
	snprintf(buf, sizeof(buf), "%s: since_bit (%%ld)", what);
	diff_eq_int(buf, ours->since_bit, ref->since_bit, tag);
	snprintf(buf, sizeof(buf), "%s: disagreements (%%ld)", what);
	diff_eq_int(buf, ours->disagreements, ref->disagreements, tag);
	snprintf(buf, sizeof(buf), "%s: hist_idx (%%ld)", what);
	diff_eq_int(buf, ours->hist_idx, ref->hist_idx, tag);
	snprintf(buf, sizeof(buf), "%s: last_count (%%ld)", what);
	diff_eq_int(buf, ours->last_count, ref->last_count, tag);

	snprintf(buf, sizeof(buf), "%s: fir_hist[%%ld]", what);
	for (i = 0; i < ours->cfg.fir_taps; i++)
		diff_eq_int(buf, ours->fir_hist[i], ref->fir_hist[i], i);
	snprintf(buf, sizeof(buf), "%s: iir_hist[%%ld]", what);
	for (i = 0; i < ours->cfg.iir_len * 2; i++)
		diff_eq_int(buf, ours->iir_hist[i], ref->iir_hist[i], i);
}

/*
 * Run one fragment through both demodulators.  `n` samples at 2400 Hz.
 * Records what the reference did, so the coverage guards below mean something.
 */
static void
demod_frag(const char *what, struct fpm_fsd *fa, struct fpm_fsd *fb,
	   const short *in, int n, int tag)
{
	unsigned short ba[64], bb[64];
	char buf[96];
	short na, nb;
	int before_bit = fa->bit;
	int i;

	memset(ba, 0xa5, sizeof(ba));
	memset(bb, 0xa5, sizeof(bb));

	na = ref_FPM_FSD_demodulate(fa, in, ba, (unsigned short)n);
	nb = FPM_FSD_demodulate(fb, in, bb, (unsigned short)n);

	snprintf(buf, sizeof(buf), "%s: bit count (%%ld)", what);
	diff_eq_int(buf, nb, na, tag);

	snprintf(buf, sizeof(buf), "%s: bit[%%ld]", what);
	for (i = 0; i < na; i++) {
		diff_eq_int(buf, bb[i], ba[i], i);
		bits_emitted++;
		if (ba[i] == 0 || ba[i] == 1)
			saw_bit[ba[i]]++;
	}

	/* Nothing written past the reported count. */
	snprintf(buf, sizeof(buf), "%s: no write past count at [%%ld]", what);
	for (i = na; i < na + 4 && i < 64; i++)
		diff_eq_int(buf, bb[i], (unsigned short)0xa5a5, i);

	/* Which path emitted?  A committed-bit change is a resync. */
	if (na > 0) {
		if (fa->bit != before_bit)
			saw_resync++;
		else
			saw_freerun++;
	}
	if (na > fa->cfg.max_bits + 1)
		saw_cap_hit++;

	compare_demod(what, fb, fa, tag);
}

int
main(void)
{
	struct fpm_fsd a, b;
	struct fpm_fsd_cfg cfg;
	int rc = 0;

	/* Bell 103's configuration, as B103FP_create builds it. */
	memset(&cfg, 0, sizeof(cfg));
	cfg.fir = ref_B103_CHAN_INTRP;
	cfg.fir_taps = 15;
	cfg.delay = 4;
	cfg.iir = ref_B103_IIR_LPF;
	cfg.iir_len = 3;
	cfg.bit_samples = 8;
	cfg.trace_len = 12;

	memset(&a, 0, sizeof(a));
	memset(&b, 0, sizeof(b));

	diff_begin("FSD init fresh");
	ref_FPM_FSD_init(&a, &cfg, 1);
	FPM_FSD_init(&b, &cfg, 1);
	compare(&b, &a);
	rc |= diff_end();

	/* Re-init in place must not reallocate, and must re-clear. */
	diff_begin("FSD init in place");
	{
		short *keep = b.fir_hist;

		b.fir_hist[0] = 0x1234;
		a.fir_hist[0] = 0x1234;
		ref_FPM_FSD_init(&a, &cfg, 0);
		FPM_FSD_init(&b, &cfg, 0);
		compare(&b, &a);
		diff_eq_int("buffer not reallocated (%ld)",
			    b.fir_hist == keep, 1, 0);
		diff_eq_int("history re-cleared (%ld)", b.fir_hist[0], 0, 0);
	}
	rc |= diff_end();

	ref_FPM_FSD_free(&a);
	FPM_FSD_free(&b);

	/* --- FPM_FSD_demodulate, on a real signal --------------------- */
	{
		struct b103fp *src = ref_B103FP_create(0, ref_B103_CFG);
		struct b103fp *rx = ref_B103FP_create(0, ref_B103_CFG);
		struct b103fp *pa = ref_B103FP_create(0, ref_B103_CFG);
		struct b103fp *pb = ref_B103FP_create(0, ref_B103_CFG);

		diff_begin("FSD demodulate objects");
		diff_eq_int("four objects built (%ld)",
			    src && rx && pa && pb, 1, 0);
		rc |= diff_end();

		if (src && rx && pa && pb) {
			static short air[1024], down[512];
			unsigned short txbits[8];
			int f, i, n8, n24;

			diff_begin("FSD demodulate on Bell 103 FSK");
			for (f = 0; f < 400; f++) {
				/*
				 * Six bits of a pattern with plenty of
				 * transitions AND some runs, so both the
				 * resync path and the free-running path get
				 * used.  0x2d3 is 1011010011.
				 */
				for (i = 0; i < 6; i++)
					txbits[i] =
						(unsigned short)((0x2d3u >> ((f * 6 + i) % 10)) & 1);

				n8 = ref_ModDataB103(src, txbits, air, 6);

				/* The receive front end, as DemodDataB103
				 * runs it: 3:10 down to 2400, then AGC. */
				n24 = ref_FPM_MRF_filter(&rx->dsp->rx_mrf, air,
							 down, (short)n8);
				ref_FPM_AGC_agc(&rx->dsp->agc, down,
						(unsigned short)n24);

				demod_frag("b103", &pa->dsp->fsd, &pb->dsp->fsd,
					   down, n24, f);
			}
			rc |= diff_end();

			/*
			 * Fragment boundaries landing mid-symbol: feed the
			 * same stream in ragged pieces so the bit clock has
			 * to carry across calls.
			 */
			diff_begin("FSD demodulate ragged fragments");
			for (f = 0; f < 200; f++) {
				int off = 0, piece;

				for (i = 0; i < 6; i++)
					txbits[i] =
						(unsigned short)((0x2d3u >> ((f * 6 + i) % 10)) & 1);
				n8 = ref_ModDataB103(src, txbits, air, 6);
				n24 = ref_FPM_MRF_filter(&rx->dsp->rx_mrf, air,
							 down, (short)n8);
				ref_FPM_AGC_agc(&rx->dsp->agc, down,
						(unsigned short)n24);

				while (off < n24) {
					piece = (f % 7) + 1;
					if (off + piece > n24)
						piece = n24 - off;
					demod_frag("ragged", &pa->dsp->fsd,
						   &pb->dsp->fsd,
						   down + off, piece, f);
					off += piece;
				}
			}
			rc |= diff_end();

			/*
			 * Over-long fragments, to reach the output cap.  Bell
			 * 103 caps at 8 bits, which needs more than 64 samples
			 * at 8 per bit -- more than one call ever gets in
			 * normal use, so nothing above would reach it.
			 */
			diff_begin("FSD demodulate output cap");
			for (f = 0; f < 60; f++) {
				int total = 0;

				/* 30 bits' worth, accumulated at 2400 Hz. */
				for (n24 = 0; total < 300; ) {
					for (i = 0; i < 6; i++)
						txbits[i] = (unsigned short)
							((0x2d3u >> ((f * 6 + i) % 10)) & 1);
					n8 = ref_ModDataB103(src, txbits, air, 6);
					n24 = ref_FPM_MRF_filter(
						&rx->dsp->rx_mrf, air,
						down + total, (short)n8);
					ref_FPM_AGC_agc(&rx->dsp->agc,
							down + total,
							(unsigned short)n24);
					total += n24;
				}
				demod_frag("capped", &pa->dsp->fsd,
					   &pb->dsp->fsd, down, total, f);
			}
			rc |= diff_end();

			ref_B103FP_delete(src);
			ref_B103FP_delete(rx);
			ref_B103FP_delete(pa);
			ref_B103FP_delete(pb);
		}
	}

	/*
	 * Anti-vacuity.  A demodulator stuck on one output would agree with a
	 * reconstruction stuck on the same one, so each path has to be shown
	 * to have run.
	 */
	diff_begin("FSD demodulate coverage");
	diff_eq_int("bits emitted (%ld)", bits_emitted > 100, 1, bits_emitted);
	diff_eq_int("zero bits seen (%ld)", saw_bit[0] > 0, 1, saw_bit[0]);
	diff_eq_int("one bits seen (%ld)", saw_bit[1] > 0, 1, saw_bit[1]);
	diff_eq_int("resync path taken (%ld)", saw_resync > 0, 1, saw_resync);
	diff_eq_int("free-running path taken (%ld)",
		    saw_freerun > 0, 1, saw_freerun);
	diff_eq_int("output cap reached (%ld)", saw_cap_hit > 0, 1, saw_cap_hit);
	rc |= diff_end();

	printf("t_fpm_fsd: %d bits (%d zero, %d one), %d resync, %d free-run, "
	       "%d capped\n", bits_emitted, saw_bit[0], saw_bit[1],
	       saw_resync, saw_freerun, saw_cap_hit);

	return rc;
}

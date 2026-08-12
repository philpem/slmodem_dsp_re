/*
 * t_v32cfg.c -- differential test of V.32's AGC and resampler configurations.
 *
 * Three layers, because comparing bytes against `ref_` proves the bytes and
 * proves nothing about the SHAPE:
 *
 *   1. field by field against the reference object, with the two pointer
 *      fields compared by what they point AT rather than by address;
 *   2. the coefficient table element by element, all 360 of them;
 *   3. and then both configurations are USED -- `FPM_MRF_init` +
 *      `FPM_MRF_filter` and `FPM_AGC_init` + `FPM_AGC_agc`, on our copy and
 *      on the reference's, over a real signal.  If a field boundary were
 *      wrong the values could still match while the block behaved
 *      differently, and only this layer would see it.
 *
 * `ref_AGC_DEF_ALPHA` is NOT declared here and must not be: the name is
 * file-static in five translation units and global in a sixth, so a test
 * naming it binds to whichever the linker picks (docs/deviations.md, D6's
 * entry in the blind-spot list).  Reaching the pair through
 * `ref_AGCv32_CFG.alpha` is unambiguous, and it is what the datapump does.
 */

#include <string.h>

#include "harness.h"
#include "dsplib/fpm_agc.h"
#include "dsplib/fpm_mrf.h"
#include "dsplib/v32cfg.h"

extern struct fpm_agc_cfg ref_AGCv32_CFG;
extern struct fpm_agc_cfg ref_AGCv32Prc_CFG;
extern const struct fpm_mrf_cfg ref_MRFv32_CFG;
extern const short ref_MRFv32_COFFS[360];

extern void ref_FPM_MRF_init(void *state, const void *cfg, int fresh);
extern short ref_FPM_MRF_filter(void *state, const short *in, short *out,
				short count);
extern void ref_FPM_MRF_free(void *state);
extern void ref_FPM_AGC_init(void *agc, const void *cfg, int reset);
extern void ref_FPM_AGC_agc(void *agc, short *buf, unsigned short count);

static void
agc_cfg(const char *tag, const struct fpm_agc_cfg *ours,
	const struct fpm_agc_cfg *ref)
{
	int i;

	(void)tag;
	diff_eq_int("ref_level (%ld)", ours->ref_level, ref->ref_level, 0);
	diff_eq_int("acquire_level (%ld)", ours->acquire_level,
		    ref->acquire_level, 0);
	diff_eq_int("squelch_level (%ld)", ours->squelch_level,
		    ref->squelch_level, 0);
	diff_eq_int("f06 (%ld)", ours->f06, ref->f06, 0);
	diff_eq_int("f08 (%ld)", ours->f08, ref->f08, 0);
	diff_eq_int("block_len (%ld)", ours->block_len, ref->block_len, 0);
	diff_eq_int("f14 (%ld)", ours->f14, ref->f14, 0);
	/* +0x16 is not padding in V.32's copies, unlike Bell 103's. */
	diff_eq_int("f16 (%ld)", ours->f16, ref->f16, 0);

	diff_eq_int("alpha is not null (%ld)", ours->alpha != 0, 1, 0);
	diff_eq_int("beta is not null (%ld)", ours->beta != 0, 1, 0);
	for (i = 0; i < 2; i++) {
		diff_eq_int("alpha[%ld]", ours->alpha[i], ref->alpha[i], i);
		diff_eq_int("beta[%ld]", ours->beta[i], ref->beta[i], i);
	}
	/*
	 * The property D6 is about, asserted on the object rather than
	 * inherited from the register: this pair sums to unity in BOTH
	 * elements, so V.32's is not one of the broken ones.
	 */
	diff_eq_int("alpha[0]+beta[0] is unity (%ld)",
		    ref->alpha[0] + ref->beta[0], 32768, 0);
	diff_eq_int("alpha[1]+beta[1] is unity (%ld)",
		    ref->alpha[1] + ref->beta[1], 32768, 0);
}

/* A signal with something to gain-control and something to resample. */
static void
excite(short *buf, int n, int seed)
{
	int i;
	long x = seed;

	for (i = 0; i < n; i++) {
		x = x * 1103515245 + 12345;
		buf[i] = (short)((int)((x >> 16) & 0x7fff) - 16384) / 8;
	}
}

int
main(void)
{
	static short in[2048], out_a[4096], out_b[4096];
	static short gain_a[2048], gain_b[2048];
	int i;

	diff_begin("AGCv32_CFG and AGCv32Prc_CFG, field by field");
	agc_cfg("AGCv32_CFG", &AGCv32_CFG, &ref_AGCv32_CFG);
	agc_cfg("AGCv32Prc_CFG", &AGCv32Prc_CFG, &ref_AGCv32Prc_CFG);
	/* The two differ in exactly two fields, and this says which. */
	diff_eq_int("the two configs differ in ref_level (%ld)",
		    AGCv32_CFG.ref_level != AGCv32Prc_CFG.ref_level, 1, 0);
	diff_eq_int("and in block_len (%ld)",
		    AGCv32_CFG.block_len != AGCv32Prc_CFG.block_len, 1, 0);
	diff_eq_int("and share both coefficient objects (%ld)",
		    AGCv32_CFG.alpha == AGCv32Prc_CFG.alpha
		    && AGCv32_CFG.beta == AGCv32Prc_CFG.beta, 1, 0);
	if (diff_end())
		return 1;

	diff_begin("MRFv32_CFG and its 360 coefficients");
	diff_eq_int("branches (%ld)", MRFv32_CFG.branches,
		    ref_MRFv32_CFG.branches, 0);
	diff_eq_int("decimate (%ld)", MRFv32_CFG.decimate,
		    ref_MRFv32_CFG.decimate, 0);
	diff_eq_int("taps (%ld)", MRFv32_CFG.taps, ref_MRFv32_CFG.taps, 0);
	diff_eq_int("pad0a (%ld)", MRFv32_CFG.pad0a, ref_MRFv32_CFG.pad0a, 0);
	diff_eq_int("aux is null (%ld)", MRFv32_CFG.aux == 0,
		    ref_MRFv32_CFG.aux == 0, 0);
	diff_eq_int("coeff points at our table (%ld)",
		    MRFv32_CFG.coeff == MRFv32_COFFS, 1, 0);
	diff_eq_int("the reference's points at its own (%ld)",
		    ref_MRFv32_CFG.coeff == ref_MRFv32_COFFS, 1, 0);
	/* 9 branches of 40 taps: the ratio and the length have to agree. */
	diff_eq_int("taps == branches * 40 (%ld)",
		    MRFv32_CFG.taps, MRFv32_CFG.branches * 40, 0);
	for (i = 0; i < 360; i++)
		diff_eq_int("MRFv32_COFFS[%ld]", MRFv32_COFFS[i],
			    ref_MRFv32_COFFS[i], i);
	if (diff_end())
		return 1;

	/*
	 * Layer 3.  Drive both blocks with both configurations.  This is what
	 * catches a field boundary that is wrong in a way the values alone
	 * cannot show.
	 */
	diff_begin("MRFv32_CFG resamples 8000 -> 7200 identically");
	{
		struct fpm_mrf a, b;
		short na, nb;
		int chunk;
		int fed = 0;

		memset(&a, 0, sizeof(a));
		memset(&b, 0, sizeof(b));
		ref_FPM_MRF_init(&a, &ref_MRFv32_CFG, 1);
		FPM_MRF_init(&b, &MRFv32_CFG, 1);
		excite(in, 2048, 7);

		/* Ragged chunks, so `need` has to carry across calls. */
		for (chunk = 1; fed + chunk <= 2048; chunk += 7) {
			memset(out_a, 0x5a, sizeof(out_a));
			memset(out_b, 0x5a, sizeof(out_b));
			na = ref_FPM_MRF_filter(&a, in + fed, out_a,
						(short)chunk);
			nb = FPM_MRF_filter(&b, in + fed, out_b, (short)chunk);
			diff_eq_int("output count at %ld", nb, na, fed);
			for (i = 0; i < na && i < nb; i++)
				diff_eq_int("sample %ld", out_b[i], out_a[i],
					    fed + i);
			fed += chunk;
		}
		diff_eq_int("something was resampled (%ld)", fed > 1000, 1,
			    fed);
		/*
		 * Field by field, not diff_eq_obj: `cfg.coeff` and `history`
		 * are addresses and the two sides' can never agree.
		 */
		diff_eq_int("need after the run (%ld)", b.need, a.need, 0);
		diff_eq_int("phase after the run (%ld)", b.phase, a.phase, 0);
		diff_eq_int("widx after the run (%ld)", b.widx, a.widx, 0);
		diff_eq_int("history_len (%ld)", b.history_len, a.history_len,
			    0);
		for (i = 0; i < b.history_len && i < a.history_len; i++)
			diff_eq_int("history[%ld]", b.history[i], a.history[i],
				    i);
		ref_FPM_MRF_free(&a);
		FPM_MRF_free(&b);
	}
	if (diff_end())
		return 1;

	diff_begin("both AGC configurations run identically");
	{
		struct fpm_agc a, b;
		int pass;
		int cfgn;

		for (cfgn = 0; cfgn < 2; cfgn++) {
			const struct fpm_agc_cfg *mine =
				cfgn ? &AGCv32Prc_CFG : &AGCv32_CFG;
			const struct fpm_agc_cfg *theirs =
				cfgn ? &ref_AGCv32Prc_CFG : &ref_AGCv32_CFG;

			memset(&a, 0, sizeof(a));
			memset(&b, 0, sizeof(b));
			ref_FPM_AGC_init(&a, theirs, 1);
			FPM_AGC_init(&b, mine, 1);

			for (pass = 0; pass < 8; pass++) {
				excite(gain_a, 720, pass + 1);
				memcpy(gain_b, gain_a, sizeof(short) * 720);
				ref_FPM_AGC_agc(&a, gain_a, 720);
				FPM_AGC_agc(&b, gain_b, 720);
				for (i = 0; i < 720; i++)
					diff_eq_int("gained sample %ld",
						    gain_b[i], gain_a[i], i);
				/*
				 * Not diff_eq_obj: the copied config carries
				 * the alpha and beta POINTERS at +0x0c and
				 * +0x10, and those are addresses.
				 */
				diff_eq_int("level, pass %ld", b.level,
					    a.level, pass);
				diff_eq_int("mult, pass %ld", b.mult, a.mult,
					    pass);
				diff_eq_int("shift, pass %ld", b.shift,
					    a.shift, pass);
				diff_eq_int("signal, pass %ld", b.signal,
					    a.signal, pass);
				diff_eq_int("freeze, pass %ld", b.freeze,
					    a.freeze, pass);
				diff_eq_int("f18, pass %ld", b.f18, a.f18,
					    pass);
			}
			/* It must actually have moved, or this proves little. */
			diff_eq_int("a gain was computed (%ld)", b.mult != 0,
				    1, cfgn);
		}
	}
	return diff_end();
}
